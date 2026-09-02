#pragma once

#include <cstddef>
#include <utility>

#include <engine/core/task.hpp>
#include <engine/core/types.hpp>

namespace engine::core {

class TaskGraph;
struct TaskNode;

/// @brief Lightweight, move-only 64-byte handle to a task within a TaskGraph.
///
/// Supports declarative DAG dependency building: .precede(), .succeed(), and
/// .then() continuations.
template <typename T = void> class alignas(64) TaskHandle {
public:
  TaskHandle() noexcept = default;
  TaskHandle(TaskNode *node, TaskGraph *graph) noexcept
      : m_node(node), m_graph(graph) {}
  ~TaskHandle() = default;

  TaskHandle(const TaskHandle &) = delete;
  TaskHandle &operator=(const TaskHandle &) = delete;

  TaskHandle(TaskHandle &&other) noexcept
      : m_node(other.m_node), m_graph(other.m_graph) {
    other.m_node = nullptr;
    other.m_graph = nullptr;
  }

  TaskHandle &operator=(TaskHandle &&other) noexcept {
    if (this != &other) {
      m_node = other.m_node;
      m_graph = other.m_graph;
      other.m_node = nullptr;
      other.m_graph = nullptr;
    }
    return *this;
  }

  /// @brief Declares that this task must complete before the successor task
  /// starts.
  template <typename U>
  void precede(const TaskHandle<U> &successor) const noexcept;

  /// @brief Declares that this task depends on the predecessor task completing
  /// first.
  template <typename U>
  void succeed(const TaskHandle<U> &predecessor) const noexcept;

  /// @brief Creates a successor task continuation that executes once this task
  /// completes. If this task returns a value T, the continuation callable
  /// receives it via std::move(result).
  template <typename F> auto then(F &&callable);

  [[nodiscard]] bool isValid() const noexcept {
    return m_node != nullptr && m_graph != nullptr;
  }

  explicit operator bool() const noexcept { return isValid(); }

  [[nodiscard]] TaskNode *getNode() const noexcept { return m_node; }

  [[nodiscard]] TaskGraph *getGraph() const noexcept { return m_graph; }

private:
  TaskNode *m_node{nullptr};            // 8 bytes
  TaskGraph *m_graph{nullptr};          // 8 bytes
  alignas(8) std::byte m_padding[48]{}; // 48 bytes padding to 64 bytes
};
static_assert(sizeof(TaskHandle<void>) == 64,
              "TaskHandle<void> must occupy exactly 64 bytes (1 cache line)");
static_assert(sizeof(TaskHandle<int>) == 64,
              "TaskHandle<T> must occupy exactly 64 bytes (1 cache line)");
static_assert(alignof(TaskHandle<void>) == 64,
              "TaskHandle must be 64-byte cache line aligned");

using AnyTaskHandle = TaskHandle<void>;

} // namespace engine::core
