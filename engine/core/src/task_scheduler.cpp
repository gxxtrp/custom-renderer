#include <engine/core/task_scheduler.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>

#if defined(_MSC_VER) || defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#endif

#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>

namespace engine::core {

namespace {

thread_local i32 t_threadIndex = -1;

void setThreadAffinity(std::thread::native_handle_type handle,
                       u32 coreIndex) noexcept {
#if defined(_WIN32)
  const DWORD_PTR mask = 1ULL << (coreIndex % 64);
  SetThreadAffinityMask(handle, mask);
#elif defined(__linux__)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreIndex, &cpuset);
  pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
#else
  (void)handle;
  (void)coreIndex;
#endif
}

inline void cpuPause() noexcept {
#if defined(_MSC_VER) || defined(__x86_64__) || defined(_M_X64)
  _mm_pause();
#else
  std::this_thread::yield();
#endif
}

} // namespace

TaskScheduler::TaskScheduler(u32 workerCount) {
  if (workerCount == 0) {
    const u32 hwCores = std::thread::hardware_concurrency();
    // Leave 1 core for platform / OS if possible, minimum 1 worker
    m_workerCount = (hwCores > 1) ? (hwCores - 1) : 1;
  } else {
    m_workerCount = workerCount;
  }

  m_totalThreadCount = m_workerCount + 1; // Workers + 1 calling thread slot

  m_deques.reserve(m_totalThreadCount);
  for (u32 i = 0; i < m_totalThreadCount; ++i) {
    m_deques.push_back(std::make_unique<WorkStealingDeque>(
        WorkStealingDeque::defaultCapacity));
  }

  m_workerExecutionCounts = std::vector<std::atomic<u64>>(m_totalThreadCount);
  for (u32 i = 0; i < m_totalThreadCount; ++i) {
    m_workerExecutionCounts[i].store(0, std::memory_order_relaxed);
  }
}

TaskScheduler::~TaskScheduler() { shutdown(); }

void TaskScheduler::init() {
  if (m_running.load(std::memory_order_relaxed)) {
    return;
  }

  m_running.store(true, std::memory_order_release);
  m_workers.reserve(m_workerCount);

  for (u32 i = 0; i < m_workerCount; ++i) {
    m_workers.emplace_back([this, i]() { workerLoop(i); });

    // Pin worker to CPU core (worker i -> core i + 1)
    setThreadAffinity(m_workers.back().native_handle(), i + 1);
  }
}

void TaskScheduler::shutdown() {
  if (!m_running.load(std::memory_order_relaxed)) {
    return;
  }

  m_running.store(false, std::memory_order_release);
  wakeWorkers();

  for (auto &worker : m_workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  m_workers.clear();
}

void TaskScheduler::run(TaskGraph &graph) {
  if (!m_running.load(std::memory_order_relaxed)) {
    init();
  }

  graph.setScheduler(this);
  graph.compile();

  const usize taskCount = graph.getTaskCount();
  if (taskCount == 0) {
    return;
  }

  m_pendingTasks.store(static_cast<i32>(taskCount), std::memory_order_release);

  // Calling thread always operates on its dedicated slot (m_workerCount)
  t_threadIndex = static_cast<i32>(m_workerCount);

  // Push all unblocked root tasks to the current thread's deque
  const auto rootTasks = graph.getRootTasks();
  for (TaskNode *root : rootTasks) {
    m_deques[static_cast<u32>(t_threadIndex)]->pushBottom(
        &root->m_executionTask);
  }

  wakeWorkers();
}

void TaskScheduler::runAndWait(TaskGraph &graph) {
  run(graph);

  t_threadIndex = static_cast<i32>(m_workerCount);
  const u32 callerIndex = static_cast<u32>(t_threadIndex);

  // Actively execute and steal tasks on the calling thread until graph
  // completes
  u32 backoff = 0;
  const auto startTime = std::chrono::steady_clock::now();
  bool warned = false;

  while (m_pendingTasks.load(std::memory_order_acquire) > 0) {
    Task<void> *task = m_deques[callerIndex]->popBottom();
    if (!task) {
      task = stealTask(callerIndex);
    }

    if (task) {
      backoff = 0;
      task->execute();
      m_workerExecutionCounts[callerIndex].fetch_add(1,
                                                     std::memory_order_relaxed);
      m_totalExecuted.fetch_add(1, std::memory_order_relaxed);
    } else {
      if (!warned && std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - startTime)
                             .count() > 1000) {
        warned = true;
        ENGINE_LOG_WARN("TaskScheduler::runAndWait slow / stuck: "
                        "pendingTasks={}, totalExecuted={}",
                        m_pendingTasks.load(), m_totalExecuted.load());
        for (u32 d = 0; d < m_totalThreadCount; ++d) {
          ENGINE_LOG_WARN("  Deque {}: size={}, executed={}, top={}, bottom={}",
                          d, m_deques[d]->size(),
                          m_workerExecutionCounts[d].load(),
                          m_deques[d]->getTop(), m_deques[d]->getBottom());
        }
        for (TaskNode *n = graph.getHeadNode(); n != nullptr;
             n = n->m_nextInGraph) {
          if (!n->m_completed.load(std::memory_order_relaxed)) {
            ENGINE_LOG_WARN(
                "  Uncompleted Task '{}': inDegree={}, completed={}",
                n->m_name ? n->m_name : "unnamed",
                n->m_inDegree.load(std::memory_order_relaxed),
                n->m_completed.load(std::memory_order_relaxed));
          }
        }
      }

      if (backoff < 32) {
        cpuPause();
        ++backoff;
      } else if (backoff < 64) {
        std::this_thread::yield();
        ++backoff;
      } else {
        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_waitCv.wait_for(lock, std::chrono::microseconds(50), [this] {
          return m_pendingTasks.load(std::memory_order_acquire) <= 0;
        });
        backoff = 0;
      }
    }
  }

  t_threadIndex = -1;
}

void TaskScheduler::wait() {
  u32 backoff = 0;
  while (m_pendingTasks.load(std::memory_order_acquire) > 0) {
    if (backoff < 32) {
      cpuPause();
      ++backoff;
    } else if (backoff < 64) {
      std::this_thread::yield();
      ++backoff;
    } else {
      std::unique_lock<std::mutex> lock(m_cvMutex);
      m_waitCv.wait_for(lock, std::chrono::microseconds(50), [this] {
        return m_pendingTasks.load(std::memory_order_acquire) <= 0;
      });
      backoff = 0;
    }
  }
}

void TaskScheduler::pushTask(Task<void> *task) noexcept {
  assert(task != nullptr);
  if (t_threadIndex < 0 || t_threadIndex >= static_cast<i32>(m_workerCount)) {
    t_threadIndex = static_cast<i32>(m_workerCount);
  }

  const u32 index = static_cast<u32>(t_threadIndex);
  [[maybe_unused]] const bool pushed = m_deques[index]->pushBottom(task);
  assert(pushed && "WorkStealingDeque overflow during pushTask");

  wakeWorkers();
}

void TaskScheduler::onTaskCompleted() noexcept {
  if (m_pendingTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    m_waitCv.notify_all();
  }
}

u64 TaskScheduler::getWorkerExecutionCount(u32 workerId) const noexcept {
  if (workerId < m_totalThreadCount) {
    return m_workerExecutionCounts[workerId].load(std::memory_order_relaxed);
  }
  return 0;
}

void TaskScheduler::wakeWorkers() noexcept {
  m_hasWork.store(true, std::memory_order_release);
  m_workCv.notify_all();
}

Task<void> *TaskScheduler::stealTask(u32 thiefId) noexcept {
  if (m_totalThreadCount <= 1) {
    return nullptr;
  }

  // Attempt to steal from up to N-1 victims starting with pseudo-random offset
  static thread_local u32 rngState = 0x12345678 ^ (thiefId * 997 + 1);
  rngState = rngState * 1664525u + 1013904223u;

  const u32 startOffset = rngState % (m_totalThreadCount - 1);

  for (u32 attempt = 0; attempt < m_totalThreadCount - 1; ++attempt) {
    const u32 victim =
        (thiefId + 1 + startOffset + attempt) % m_totalThreadCount;
    if (victim == thiefId) {
      continue;
    }

    Task<void> *task = m_deques[victim]->stealTop();
    if (task) {
      m_totalSteals.fetch_add(1, std::memory_order_relaxed);
      return task;
    }
  }

  return nullptr;
}

void TaskScheduler::workerLoop(u32 workerId) {
  t_threadIndex = static_cast<i32>(workerId);

  char threadName[32];
  std::snprintf(threadName, sizeof(threadName), "Worker_%u", workerId);
#if defined(TRACY_ENABLE) || defined(ENGINE_PROFILING_ENABLE)
  tracy::SetThreadName(threadName);
#endif

  u32 backoff = 0;

  while (m_running.load(std::memory_order_relaxed)) {
    Task<void> *task = m_deques[workerId]->popBottom();
    if (!task) {
      task = stealTask(workerId);
    }

    if (task) {
      backoff = 0;
      task->execute();
      m_workerExecutionCounts[workerId].fetch_add(1, std::memory_order_relaxed);
      m_totalExecuted.fetch_add(1, std::memory_order_relaxed);
    } else {
      if (backoff < 32) {
        cpuPause();
        ++backoff;
      } else if (backoff < 64) {
        std::this_thread::yield();
        ++backoff;
      } else {
        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_workCv.wait_for(lock, std::chrono::microseconds(100), [this] {
          return !m_running.load(std::memory_order_relaxed) ||
                 m_hasWork.load(std::memory_order_relaxed);
        });
        backoff = 0;
      }
    }
  }
}

} // namespace engine::core
