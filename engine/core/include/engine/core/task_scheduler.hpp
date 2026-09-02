#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <engine/core/task.hpp>
#include <engine/core/task_graph.hpp>
#include <engine/core/types.hpp>
#include <engine/core/work_stealing_deque.hpp>

namespace engine::core {

/// @brief High-performance work-stealing task scheduler runtime.
///
/// Manages worker threads mapped to logical CPU cores with core affinity,
/// lock-free Chase-Lev deques, exponential backoff work stealing loops, and
/// Tracy profiling zones.
class TaskScheduler {
public:
  explicit TaskScheduler(u32 workerCount = 0);
  ~TaskScheduler();

  TaskScheduler(const TaskScheduler &) = delete;
  TaskScheduler &operator=(const TaskScheduler &) = delete;
  TaskScheduler(TaskScheduler &&) noexcept = delete;
  TaskScheduler &operator=(TaskScheduler &&) noexcept = delete;

  /// @brief Starts worker threads and pins them to logical CPU cores.
  void init();

  /// @brief Gracefully stops worker threads and releases scheduler resources.
  void shutdown();

  /// @brief Dispatches root tasks from the task graph across worker deques
  /// asynchronously.
  void run(TaskGraph &graph);

  /// @brief Dispatches the task graph and executes tasks on the calling thread
  /// until graph completes.
  void runAndWait(TaskGraph &graph);

  /// @brief Waits until all tasks in the currently running graph have
  /// completed.
  void wait();

  /// @brief Executes a synchronous parallel-for loop across worker threads and
  /// waits for completion.
  template <typename F>
  void parallelFor(usize count, usize batchSize, F &&callable) {
    TaskGraph graph(std::max<usize>(count * 64, 1024 * 1024));
    graph.parallelFor(count, batchSize, std::forward<F>(callable));
    runAndWait(graph);
  }

  /// @brief Pushes a task to the calling thread's local deque.
  void pushTask(Task<void> *task) noexcept;

  /// @brief Notification callback from TaskGraph when a task finishes.
  void onTaskCompleted() noexcept;

  [[nodiscard]] u32 getWorkerCount() const noexcept { return m_workerCount; }

  [[nodiscard]] bool isRunning() const noexcept {
    return m_running.load(std::memory_order_relaxed);
  }

  [[nodiscard]] u64 getTotalTasksExecuted() const noexcept {
    return m_totalExecuted.load(std::memory_order_relaxed);
  }

  [[nodiscard]] u64 getTotalSteals() const noexcept {
    return m_totalSteals.load(std::memory_order_relaxed);
  }

  [[nodiscard]] i32 getPendingTasks() const noexcept {
    return m_pendingTasks.load(std::memory_order_relaxed);
  }

  [[nodiscard]] usize getDequeSize(u32 index) const noexcept {
    return (index < m_deques.size()) ? m_deques[index]->size() : 0;
  }

  [[nodiscard]] u64 getWorkerExecutionCount(u32 workerId) const noexcept;

private:
  void workerLoop(u32 workerId);
  Task<void> *stealTask(u32 thiefId) noexcept;
  void wakeWorkers() noexcept;
  void executeOneTask(u32 threadIndex) noexcept;

  u32 m_workerCount{0};
  u32 m_totalThreadCount{0}; // m_workerCount workers + 1 main thread slot

  std::vector<std::thread> m_workers;
  std::vector<std::unique_ptr<WorkStealingDeque>> m_deques;
  std::vector<std::atomic<u64>> m_workerExecutionCounts;

  std::atomic<bool> m_running{false};
  std::atomic<i32> m_pendingTasks{0};
  std::atomic<u64> m_totalExecuted{0};
  std::atomic<u64> m_totalSteals{0};

  std::mutex m_cvMutex;
  std::condition_variable m_workCv;
  std::condition_variable m_waitCv;
  std::atomic<bool> m_hasWork{false};
};

} // namespace engine::core
