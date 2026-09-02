#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

#include <engine/core/linear_allocator.hpp>
#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/task.hpp>
#include <engine/core/task_graph.hpp>
#include <engine/core/task_handle.hpp>
#include <engine/core/task_scheduler.hpp>
#include <engine/core/types.hpp>
#include <engine/core/work_stealing_deque.hpp>
#include <engine/platform/platform.hpp>

#include <tests/common/timer.hpp>

namespace {

// Global allocation tracking for Phase 6 (Zero heap allocation in steady-state)
std::atomic<engine::core::usize> g_steadyStateAllocations{0};
std::atomic<bool> g_trackAllocations{false};

} // namespace

void *operator new(std::size_t size) {
  if (g_trackAllocations.load(std::memory_order_relaxed)) {
    g_steadyStateAllocations.fetch_add(1, std::memory_order_relaxed);
  }
  void *ptr = std::malloc(size ? size : 1);
  if (!ptr) {
    std::abort();
  }
  return ptr;
}

void operator delete(void *ptr) noexcept { std::free(ptr); }

void operator delete(void *ptr, std::size_t /*size*/) noexcept {
  std::free(ptr);
}

void *operator new[](std::size_t size) {
  if (g_trackAllocations.load(std::memory_order_relaxed)) {
    g_steadyStateAllocations.fetch_add(1, std::memory_order_relaxed);
  }
  void *ptr = std::malloc(size ? size : 1);
  if (!ptr) {
    std::abort();
  }
  return ptr;
}

void operator delete[](void *ptr) noexcept { std::free(ptr); }

void operator delete[](void *ptr, std::size_t /*size*/) noexcept {
  std::free(ptr);
}

namespace {

using namespace engine::core;

void runPhase1_WorkStealingDeque() {
  ENGINE_LOG_INFO("--- Phase 1: WorkStealingDeque Concurrency Stress Test ---");

  static_assert(alignof(WorkStealingDeque) == 64,
                "WorkStealingDeque must be 64-byte cache line aligned");

  // Test 1: Single-thread LIFO push and pop
  {
    WorkStealingDeque deque(1024);
    assert(deque.isEmpty());
    assert(deque.size() == 0);

    constexpr usize count = 100;
    std::vector<Task<void>> tasks;
    tasks.reserve(count);

    for (usize i = 0; i < count; ++i) {
      tasks.emplace_back([]() noexcept {});
      [[maybe_unused]] const bool pushed = deque.pushBottom(&tasks.back());
      assert(pushed);
    }

    assert(deque.size() == count);

    // LIFO pop order check
    for (usize i = 0; i < count; ++i) {
      [[maybe_unused]] Task<void> *popped = deque.popBottom();
      assert(popped == &tasks[count - 1 - i]);
    }

    assert(deque.isEmpty());
    assert(deque.popBottom() == nullptr);
  }

  // Test 2: Multi-threaded thief contention race
  {
    constexpr usize totalTasks = 10000;
    WorkStealingDeque deque(16384);

    std::vector<Task<void>> tasks;
    tasks.reserve(totalTasks);

    std::atomic<usize> executedCount{0};
    for (usize i = 0; i < totalTasks; ++i) {
      tasks.emplace_back([&executedCount]() noexcept {
        executedCount.fetch_add(1, std::memory_order_relaxed);
      });
      [[maybe_unused]] const bool pushed = deque.pushBottom(&tasks.back());
      assert(pushed);
    }

    std::atomic<bool> startThieves{false};
    std::atomic<bool> done{false};
    constexpr usize thiefCount = 4;
    std::vector<std::thread> thieves;
    thieves.reserve(thiefCount);

    for (usize t = 0; t < thiefCount; ++t) {
      thieves.emplace_back([&]() {
        while (!startThieves.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }

        while (!done.load(std::memory_order_acquire) || !deque.isEmpty()) {
          Task<void> *stolen = deque.stealTop();
          if (stolen) {
            stolen->execute();
          } else {
            std::this_thread::yield();
          }
        }
      });
    }

    startThieves.store(true, std::memory_order_release);

    // Owner pops from bottom concurrently while thieves steal from top
    while (!deque.isEmpty()) {
      Task<void> *popped = deque.popBottom();
      if (popped) {
        popped->execute();
      }
    }

    done.store(true, std::memory_order_release);

    for (auto &thief : thieves) {
      thief.join();
    }

    assert(executedCount.load(std::memory_order_relaxed) == totalTasks);
    ENGINE_LOG_INFO("  [OK] WorkStealingDeque: {} tasks executed under high "
                    "multi-threaded contention (0 lost, 0 duplicates)",
                    totalTasks);
  }
}

void runPhase2_TaskMoveAndContinuations() {
  ENGINE_LOG_INFO(
      "--- Phase 2: Task 64-byte Move Primitives & Typed Continuations ---");

  static_assert(sizeof(Task<void>) == 64,
                "Task<void> must be exactly 64 bytes");
  static_assert(alignof(Task<void>) == 64,
                "Task<void> must be 64-byte aligned");
  static_assert(sizeof(Task<int>) == 64, "Task<int> must be exactly 64 bytes");
  static_assert(alignof(Task<int>) == 64, "Task<int> must be 64-byte aligned");
  static_assert(sizeof(TaskHandle<void>) == 64,
                "TaskHandle<void> must be exactly 64 bytes");
  static_assert(sizeof(TaskHandle<int>) == 64,
                "TaskHandle<int> must be exactly 64 bytes");

  // Move-only task execution
  {
    bool executed = false;
    Task<void> task([&executed]() noexcept { executed = true; });

    assert(task.isValid());
    Task<void> movedTask(std::move(task));
    assert(!task.isValid());
    assert(movedTask.isValid());

    movedTask.execute();
    assert(executed);
  }

  // Typed return value and continuation passing via std::move
  {
    TaskGraph graph(1024 * 1024);

    auto taskA = graph.createTask("Producer_A", []() -> int { return 21; });

    auto taskB = taskA.then([](int val) -> int { return val * 2; });

    int finalResult = 0;
    auto taskC =
        taskB.then([&finalResult](int val) { finalResult = val + 10; });
    (void)taskC;

    TaskScheduler scheduler(2);
    scheduler.runAndWait(graph);

    assert(finalResult == 52); // (21 * 2) + 10 = 52
    ENGINE_LOG_INFO(
        "  [OK] Typed task continuation: chain a(21) -> b(*2) -> c(+10) = {}",
        finalResult);
    scheduler.shutdown();
  }
}

void runPhase3_ComplexDAGDependencies() {
  ENGINE_LOG_INFO("--- Phase 3: Complex Task DAG Dependencies ---");

  TaskScheduler scheduler(4);
  scheduler.init();

  // Diamond DAG: 1 Root -> 4 Branches -> 1 Middle Join -> 4 Branches -> 1 Final
  // Join
  {
    TaskGraph graph(1024 * 1024);

    std::atomic<i32> stage1Sum{0};
    std::atomic<i32> stage2Sum{0};
    std::atomic<bool> middleJoinExecuted{false};
    std::atomic<bool> finalJoinExecuted{false};

    auto root = graph.createTask("Root", []() noexcept {});

    auto midJoin = graph.createTask("MiddleJoin", [&]() noexcept {
      assert(stage1Sum.load(std::memory_order_relaxed) == 40);
      middleJoinExecuted.store(true, std::memory_order_release);
    });

    for (i32 i = 0; i < 4; ++i) {
      auto b = graph.createTask("Stage1_Branch", [&stage1Sum]() noexcept {
        stage1Sum.fetch_add(10, std::memory_order_relaxed);
      });
      root.precede(b);
      b.precede(midJoin);
    }

    auto finalJoin = graph.createTask("FinalJoin", [&]() noexcept {
      assert(middleJoinExecuted.load(std::memory_order_acquire));
      assert(stage2Sum.load(std::memory_order_relaxed) == 400);
      finalJoinExecuted.store(true, std::memory_order_release);
    });

    for (i32 i = 0; i < 4; ++i) {
      auto b = graph.createTask("Stage2_Branch", [&stage2Sum]() noexcept {
        stage2Sum.fetch_add(100, std::memory_order_relaxed);
      });
      midJoin.precede(b);
      b.precede(finalJoin);
    }

    scheduler.runAndWait(graph);

    assert(middleJoinExecuted.load(std::memory_order_relaxed));
    assert(finalJoinExecuted.load(std::memory_order_relaxed));
    ENGINE_LOG_INFO("  [OK] Multi-stage Diamond DAG executed successfully with "
                    "zero deadlocks");
  }

  // Deep linear chain (50 consecutive continuations)
  {
    TaskGraph graph(1024 * 1024);

    std::atomic<u32> chainCounter{0};
    auto curr = graph.createTask("Chain_0", [&chainCounter]() noexcept {
      chainCounter.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr u32 chainDepth = 50;
    for (u32 i = 1; i < chainDepth; ++i) {
      curr = curr.then([&chainCounter]() noexcept {
        chainCounter.fetch_add(1, std::memory_order_relaxed);
      });
    }

    scheduler.runAndWait(graph);
    assert(chainCounter.load(std::memory_order_relaxed) == chainDepth);
    ENGINE_LOG_INFO(
        "  [OK] Deep continuation chain of depth {} completed cleanly",
        chainDepth);
  }

  scheduler.shutdown();
}

void runPhase4_ParallelForBenchmark() {
  ENGINE_LOG_INFO("--- Phase 4: 100,000+ Task parallelFor Benchmark ---");

  constexpr usize elementCount = 100000;
  std::vector<u32> data(elementCount);
  for (usize i = 0; i < elementCount; ++i) {
    data[i] = static_cast<u32>(i);
  }

  TaskScheduler scheduler(0); // Use all hardware cores
  scheduler.init();
  const u32 workers = scheduler.getWorkerCount();

  ENGINE_LOG_INFO(
      "  Executing parallelFor over {} elements on {} worker threads...",
      elementCount, workers);

  tests::common::Timer timer;
  timer.reset();

  TaskGraph graph(4 * 1024 * 1024);
  constexpr usize batchSize = 1024;

  graph.parallelFor("transform_100k", elementCount, batchSize,
                    [&data](usize i) noexcept { data[i] = data[i] * 3u + 7u; });

  scheduler.runAndWait(graph);
  const f32 elapsedSeconds = timer.elapsedSeconds();
  const f32 elapsedMs = elapsedSeconds * 1000.0f;

  // Verify all 100,000 array elements
  bool allValid = true;
  for (usize i = 0; i < elementCount; ++i) {
    const u32 expected = static_cast<u32>(i) * 3u + 7u;
    if (data[i] != expected) {
      allValid = false;
      break;
    }
  }

  if (!allValid) {
    std::abort();
  }
  assert(allValid && "parallelFor array transformation data mismatch!");
  const f64 throughput = (elapsedSeconds > 0.0f)
                             ? ((static_cast<f64>(elementCount) /
                                 static_cast<f64>(elapsedSeconds)) /
                                1'000'000.0)
                             : 0.0;

  ENGINE_LOG_INFO("  [OK] 100,000 elements transformed in {:.2f} ms ({:.2f} "
                  "M-items/sec) across {} cores",
                  elapsedMs, throughput, workers);

  scheduler.shutdown();
}

void runPhase5_MultiCoreWorkStealingDistribution() {
  ENGINE_LOG_INFO(
      "--- Phase 5: Multi-Core Work Stealing Load Distribution ---");

  TaskScheduler scheduler(4);
  scheduler.init();

  // Spawn 2,000 fine-grained tasks in parallel and inspect stealing
  // distribution
  constexpr usize taskCount = 2000;
  std::atomic<usize> completedCount{0};
  std::vector<std::atomic<u32>> elementExecutions(taskCount);
  for (usize i = 0; i < taskCount; ++i) {
    elementExecutions[i].store(0, std::memory_order_relaxed);
  }

  TaskGraph graph(2 * 1024 * 1024);
  graph.parallelFor("distribution_test", taskCount, 16,
                    [&completedCount, &elementExecutions](usize i) noexcept {
                      // Small computation to encourage stealing across cores
                      volatile u64 dummy = 0;
                      for (u64 k = 0; k < 500; ++k) {
                        dummy += k;
                      }
                      (void)dummy;
                      elementExecutions[i].fetch_add(1,
                                                     std::memory_order_relaxed);
                      completedCount.fetch_add(1, std::memory_order_relaxed);
                    });

  scheduler.runAndWait(graph);

  usize duplicateCount = 0;
  for (usize i = 0; i < taskCount; ++i) {
    const u32 execs = elementExecutions[i].load(std::memory_order_relaxed);
    if (execs != 1) {
      ++duplicateCount;
      if (duplicateCount <= 5) {
        ENGINE_LOG_ERROR("  Element {} executed {} times!", i, execs);
      }
    }
  }
  assert(duplicateCount == 0 &&
         "Work stealing must have zero duplicate or missing elements!");
  assert(completedCount.load(std::memory_order_relaxed) == taskCount);

  u32 participatingWorkers = 0;
  for (u32 i = 0; i < scheduler.getWorkerCount(); ++i) {
    const u64 executed = scheduler.getWorkerExecutionCount(i);
    if (executed > 0) {
      ++participatingWorkers;
    }
    ENGINE_LOG_INFO("  Worker {}: executed {} tasks", i, executed);
  }

  const u64 totalSteals = scheduler.getTotalSteals();
  ENGINE_LOG_INFO("  Total steals across all deques: {}", totalSteals);
  assert(participatingWorkers > 1 &&
         "Work stealing must balance work across multiple worker threads!");

  ENGINE_LOG_INFO("  [OK] Work stealing distributed execution across {} "
                  "workers with {} steals",
                  participatingWorkers, totalSteals);

  scheduler.shutdown();
}

void runPhase6_SteadyStateZeroAllocation() {
  ENGINE_LOG_INFO(
      "--- Phase 6: Steady-State Zero Heap Allocation Verification ---");

  TaskScheduler scheduler(4);
  scheduler.init();

  TaskGraph graph(4 * 1024 * 1024);

  // Warm-up iteration (allocating LinearAllocator buffers upfront)
  {
    graph.parallelFor("warmup", 500, 16, [](usize) noexcept {});
    scheduler.runAndWait(graph);
    graph.reset();
  }

  // Steady-state frame loop: execute 10 consecutive frames
  constexpr usize frameCount = 10;
  g_steadyStateAllocations.store(0, std::memory_order_relaxed);
  g_trackAllocations.store(true, std::memory_order_seq_cst);

  for (usize frame = 0; frame < frameCount; ++frame) {
    // Build graph with parallelFor + continuations
    auto join =
        graph.parallelFor("steady_parallel", 1000, 32, [](usize) noexcept {});
    auto cont = join.then([]() noexcept {});
    (void)cont;

    scheduler.runAndWait(graph);
    graph.reset();
  }

  g_trackAllocations.store(false, std::memory_order_seq_cst);
  const usize heapAllocs =
      g_steadyStateAllocations.load(std::memory_order_relaxed);

  ENGINE_LOG_INFO(
      "  Steady-state allocations over {} frames: {} bytes / allocs",
      frameCount, heapAllocs);
  assert(heapAllocs == 0 && "Zero heap allocations required during "
                            "steady-state task graph frame loop!");

  ENGINE_LOG_INFO("  [OK] Verified 0 runtime heap allocations across {} "
                  "steady-state frame loop iterations",
                  frameCount);

  scheduler.shutdown();
}

} // namespace

int main(int /*argc*/, char ** /*argv*/) {
  using namespace engine::core;

  Log::init(LogLevel::Info);
  ENGINE_LOG_INFO("=================================================");
  ENGINE_LOG_INFO(" Starting Scenario: m6_5_task_graph");
  ENGINE_LOG_INFO("=================================================");

  auto platformResult = engine::platform::Platform::init();
  if (!platformResult) {
    ENGINE_LOG_FATAL("Platform init failed: {}", platformResult.error());
    return 1;
  }

  runPhase1_WorkStealingDeque();
  runPhase2_TaskMoveAndContinuations();
  runPhase3_ComplexDAGDependencies();
  runPhase4_ParallelForBenchmark();
  runPhase5_MultiCoreWorkStealingDistribution();
  runPhase6_SteadyStateZeroAllocation();

  engine::platform::Platform::shutdown();

  ENGINE_LOG_INFO("=================================================");
  ENGINE_LOG_INFO(" [SUCCESS] All M6.5 Task Graph validations passed!");
  ENGINE_LOG_INFO("=================================================");
  return 0;
}
