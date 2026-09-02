#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <new>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <engine/core/linear_allocator.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/task.hpp>
#include <engine/core/task_handle.hpp>
#include <engine/core/types.hpp>

namespace engine::core {

class TaskScheduler;
class TaskGraph;

struct SuccessorLink {
  TaskNode *successor{nullptr};
  SuccessorLink *next{nullptr};
};

/// @brief Internal task node representation within a TaskGraph.
///
/// Contains the executable 64-byte Task pushed to WorkStealingDeques, atomic
/// in-degree dependency tracking, successor continuation links, and optional
/// result storage for typed task value passing.
struct alignas(64) TaskNode {
  alignas(64) Task<void> m_executionTask;            // 64 bytes (cache line 1)
  alignas(64) std::atomic<i32> m_inDegree{0};        // 4 bytes (cache line 2)
  i32 m_initialInDegree{0};                          // 4 bytes
  std::atomic<bool> m_completed{false};              // 1 byte
  Task<void> m_userTask;                             // 64 bytes (cache line 3)
  TaskNode *m_nextInGraph{nullptr};                  // 8 bytes
  SuccessorLink *m_successors{nullptr};              // 8 bytes
  const char *m_name{nullptr};                       // 8 bytes
  void *m_resultStorage{nullptr};                    // 8 bytes
  void (*m_resultDestroyer)(void *storage){nullptr}; // 8 bytes
  TaskGraph *m_graph{nullptr};                       // 8 bytes
};

/// @brief Declarative Directed Acyclic Graph (DAG) builder and runtime memory
/// container for tasks.
///
/// Uses LinearAllocator for zero steady-state heap allocations during task
/// creation, dependency building, continuation passing, and parallelFor
/// iteration.
class TaskGraph {
public:
  static constexpr usize defaultAllocatorCapacity = 4 * 1024 * 1024; // 4MB

  explicit TaskGraph(usize allocatorCapacity = defaultAllocatorCapacity);
  explicit TaskGraph(LinearAllocator &allocator);
  ~TaskGraph();

  TaskGraph(const TaskGraph &) = delete;
  TaskGraph &operator=(const TaskGraph &) = delete;

  TaskGraph(TaskGraph &&other) noexcept;
  TaskGraph &operator=(TaskGraph &&other) noexcept;

  /// @brief Creates a task node in the graph.
  template <typename F> auto createTask(F &&callable) {
    return createTask(nullptr, std::forward<F>(callable));
  }

  /// @brief Creates a named task node in the graph for Tracy profiling.
  template <typename F> auto createTask(const char *name, F &&callable) {
    using DecayedF = std::decay_t<F>;
    using ReturnType = std::invoke_result_t<DecayedF>;

    TaskNode *node = allocateNode(name);

    if constexpr (std::is_same_v<ReturnType, void>) {
      setupVoidTask(node, std::forward<F>(callable));
      return TaskHandle<void>(node, this);
    } else {
      setupTypedTask<ReturnType>(node, std::forward<F>(callable));
      return TaskHandle<ReturnType>(node, this);
    }
  }

  /// @brief Creates a continuation task depending on a typed predecessor task.
  template <typename T, typename F>
  auto createContinuation(const TaskHandle<T> &predecessor, F &&callable) {
    using DecayedF = std::decay_t<F>;
    TaskNode *predNode = predecessor.getNode();
    assert(predNode != nullptr && predNode->m_resultStorage != nullptr);

    if constexpr (std::is_invocable_v<DecayedF, T>) {
      using ReturnType = std::invoke_result_t<DecayedF, T>;
      TaskNode *node = allocateNode("Continuation");

      if constexpr (std::is_same_v<ReturnType, void>) {
        setupContinuationVoid<T>(node, predNode, std::forward<F>(callable));
        precede(predNode, node);
        return TaskHandle<void>(node, this);
      } else {
        setupContinuationTyped<T, ReturnType>(node, predNode,
                                              std::forward<F>(callable));
        precede(predNode, node);
        return TaskHandle<ReturnType>(node, this);
      }
    } else {
      static_assert(std::is_invocable_v<DecayedF, T>,
                    "Continuation callable must accept predecessor return type "
                    "T as argument");
    }
  }

  /// @brief Partitions count iterations into chunks of batchSize across worker
  /// threads with an automatic join barrier.
  template <typename F>
  TaskHandle<void> parallelFor(usize count, usize batchSize, F &&callable) {
    return parallelFor(nullptr, count, batchSize, std::forward<F>(callable));
  }

  /// @brief Partitions count iterations into chunks of batchSize with a named
  /// Tracy zone.
  template <typename F>
  TaskHandle<void> parallelFor(const char *name, usize count, usize batchSize,
                               F &&callable) {
    using DecayedF = std::decay_t<F>;
    if (count == 0) {
      return createTask(name ? name : "parallelFor_empty", []() noexcept {});
    }

    if (batchSize == 0) {
      batchSize = 1;
    }

    const usize numBatches = (count + batchSize - 1) / batchSize;
    auto joinTask =
        createTask(name ? name : "parallelFor_join", []() noexcept {});

    // Allocate persistent copy of callable in LinearAllocator to prevent
    // lifetime issues
    DecayedF *storedCallable =
        m_allocator->create<DecayedF>(std::forward<F>(callable));
    assert(storedCallable != nullptr &&
           "Failed to allocate parallelFor callable in LinearAllocator");

    for (usize batchIdx = 0; batchIdx < numBatches; ++batchIdx) {
      const usize start = batchIdx * batchSize;
      const usize end =
          (start + batchSize < count) ? (start + batchSize) : count;

      auto chunkTask = createTask("parallelFor_chunk",
                                  [start, end, storedCallable]() noexcept {
                                    for (usize i = start; i < end; ++i) {
                                      (*storedCallable)(i);
                                    }
                                  });

      chunkTask.precede(joinTask);
    }

    return joinTask;
  }

  /// @brief Declares a dependency: predecessor must complete before successor
  /// starts.
  void precede(TaskNode *predecessor, TaskNode *successor) noexcept;
  void succeed(TaskNode *successor, TaskNode *predecessor) noexcept;

  template <typename T, typename U>
  void precede(const TaskHandle<T> &predecessor,
               const TaskHandle<U> &successor) noexcept {
    precede(predecessor.getNode(), successor.getNode());
  }

  template <typename T, typename U>
  void succeed(const TaskHandle<T> &successor,
               const TaskHandle<U> &predecessor) noexcept {
    succeed(successor.getNode(), predecessor.getNode());
  }

  /// @brief Compiles the DAG, discovers root tasks (in-degree == 0), and stores
  /// initial in-degrees.
  void compile();

  /// @brief Resets the task graph, clearing all tasks and reclaiming all memory
  /// in O(1).
  void reset() noexcept;

  [[nodiscard]] std::span<TaskNode *> getRootTasks() const noexcept {
    return m_rootTasks;
  }

  [[nodiscard]] usize getTaskCount() const noexcept { return m_nodeCount; }

  [[nodiscard]] TaskNode *getHeadNode() const noexcept { return m_headNode; }

  void setScheduler(TaskScheduler *scheduler) noexcept {
    m_scheduler = scheduler;
  }

  [[nodiscard]] TaskScheduler *getScheduler() const noexcept {
    return m_scheduler;
  }

  void onTaskReady(TaskNode *node) noexcept;
  void onTaskFinished() noexcept;

private:
  TaskNode *allocateNode(const char *name);

  template <typename F> void setupVoidTask(TaskNode *node, F &&callable) {
    using DecayedF = std::decay_t<F>;
    if constexpr (sizeof(DecayedF) <= Task<void>::inlineStorageSize &&
                  alignof(DecayedF) <= Task<void>::inlineAlignment) {
      node->m_userTask = Task<void>(std::forward<F>(callable));
    } else {
      DecayedF *heaplessFn =
          m_allocator->create<DecayedF>(std::forward<F>(callable));
      assert(heaplessFn != nullptr &&
             "Failed to allocate callable in LinearAllocator");
      node->m_userTask =
          Task<void>([heaplessFn]() noexcept { (*heaplessFn)(); });
    }
    setupExecutionTask(node);
  }

  template <typename ReturnType, typename F>
  void setupTypedTask(TaskNode *node, F &&callable) {
    using DecayedF = std::decay_t<F>;
    node->m_resultStorage =
        m_allocator->allocate(sizeof(ReturnType), alignof(ReturnType));
    assert(node->m_resultStorage != nullptr &&
           "Failed to allocate result storage in LinearAllocator");

    node->m_resultDestroyer = [](void *storage) noexcept {
      static_cast<ReturnType *>(storage)->~ReturnType();
    };

    if constexpr (sizeof(DecayedF) <= 32 && alignof(DecayedF) <= 8) {
      node->m_userTask =
          Task<void>([node, fn = std::forward<F>(callable)]() mutable noexcept {
            ::new (node->m_resultStorage) ReturnType(fn());
          });
    } else {
      DecayedF *heaplessFn =
          m_allocator->create<DecayedF>(std::forward<F>(callable));
      assert(heaplessFn != nullptr &&
             "Failed to allocate callable in LinearAllocator");
      node->m_userTask = Task<void>([node, heaplessFn]() noexcept {
        ::new (node->m_resultStorage) ReturnType((*heaplessFn)());
      });
    }
    setupExecutionTask(node);
  }

  template <typename T, typename F>
  void setupContinuationVoid(TaskNode *node, TaskNode *predNode, F &&callable) {
    using DecayedF = std::decay_t<F>;
    if constexpr (sizeof(DecayedF) <= 24 && alignof(DecayedF) <= 8) {
      node->m_userTask = Task<void>(
          [predNode, fn = std::forward<F>(callable)]() mutable noexcept {
            auto *res = static_cast<T *>(predNode->m_resultStorage);
            fn(std::move(*res));
            res->~T();
          });
    } else {
      DecayedF *heaplessFn =
          m_allocator->create<DecayedF>(std::forward<F>(callable));
      assert(heaplessFn != nullptr &&
             "Failed to allocate continuation callable in LinearAllocator");
      node->m_userTask = Task<void>([predNode, heaplessFn]() noexcept {
        auto *res = static_cast<T *>(predNode->m_resultStorage);
        (*heaplessFn)(std::move(*res));
        res->~T();
      });
    }
    setupExecutionTask(node);
  }

  template <typename T, typename ReturnType, typename F>
  void setupContinuationTyped(TaskNode *node, TaskNode *predNode,
                              F &&callable) {
    using DecayedF = std::decay_t<F>;
    node->m_resultStorage =
        m_allocator->allocate(sizeof(ReturnType), alignof(ReturnType));
    assert(node->m_resultStorage != nullptr &&
           "Failed to allocate result storage in LinearAllocator");

    node->m_resultDestroyer = [](void *storage) noexcept {
      static_cast<ReturnType *>(storage)->~ReturnType();
    };

    if constexpr (sizeof(DecayedF) <= 16 && alignof(DecayedF) <= 8) {
      node->m_userTask = Task<void>(
          [node, predNode, fn = std::forward<F>(callable)]() mutable noexcept {
            auto *res = static_cast<T *>(predNode->m_resultStorage);
            ::new (node->m_resultStorage) ReturnType(fn(std::move(*res)));
            res->~T();
          });
    } else {
      DecayedF *heaplessFn =
          m_allocator->create<DecayedF>(std::forward<F>(callable));
      assert(heaplessFn != nullptr &&
             "Failed to allocate continuation callable in LinearAllocator");
      node->m_userTask = Task<void>([node, predNode, heaplessFn]() noexcept {
        auto *res = static_cast<T *>(predNode->m_resultStorage);
        ::new (node->m_resultStorage)
            ReturnType((*heaplessFn)(std::move(*res)));
        res->~T();
      });
    }
    setupExecutionTask(node);
  }

  void setupExecutionTask(TaskNode *node);

  LinearAllocator *m_ownedAllocator{nullptr};
  LinearAllocator *m_allocator{nullptr};

  TaskNode *m_headNode{nullptr};
  TaskNode *m_tailNode{nullptr};
  usize m_nodeCount{0};

  std::span<TaskNode *> m_rootTasks{};
  TaskScheduler *m_scheduler{nullptr};
  bool m_compiled{false};
};

// Definitions of TaskHandle inline methods
template <typename T>
template <typename U>
inline void
TaskHandle<T>::precede(const TaskHandle<U> &successor) const noexcept {
  assert(m_graph != nullptr && m_node != nullptr &&
         successor.getNode() != nullptr);
  m_graph->precede(m_node, successor.getNode());
}

template <typename T>
template <typename U>
inline void
TaskHandle<T>::succeed(const TaskHandle<U> &predecessor) const noexcept {
  assert(m_graph != nullptr && m_node != nullptr &&
         predecessor.getNode() != nullptr);
  m_graph->succeed(m_node, predecessor.getNode());
}

template <typename T>
template <typename F>
inline auto TaskHandle<T>::then(F &&callable) {
  assert(m_graph != nullptr && m_node != nullptr);
  if constexpr (std::is_same_v<T, void>) {
    auto next = m_graph->createTask(std::forward<F>(callable));
    this->precede(next);
    return next;
  } else {
    return m_graph->createContinuation(*this, std::forward<F>(callable));
  }
}

} // namespace engine::core
