#pragma once

#include <atomic>
#include <cassert>
#include <thread>

#include <engine/core/task.hpp>
#include <engine/core/types.hpp>

namespace engine::core {

class LinearAllocator;

/// @brief Cache-aligned, lock-free double-ended circular work-stealing ring
/// buffer (Chase-Lev deque).
///
/// The owning worker thread executes in LIFO order at bottom (maximizing L1/L2
/// cache locality) while idle thief threads steal in FIFO order at top using
/// atomic compare-and-swap.
class alignas(64) WorkStealingDeque {
public:
  static constexpr usize defaultCapacity = 16384;

  explicit WorkStealingDeque(usize capacity = defaultCapacity);
  WorkStealingDeque(usize capacity, LinearAllocator &allocator);
  ~WorkStealingDeque();

  WorkStealingDeque(const WorkStealingDeque &) = delete;
  WorkStealingDeque &operator=(const WorkStealingDeque &) = delete;
  WorkStealingDeque(WorkStealingDeque &&) noexcept = delete;
  WorkStealingDeque &operator=(WorkStealingDeque &&) noexcept = delete;

  /// @brief Pushes a task to the bottom of the deque (called only by the owning
  /// thread).
  /// @return true if pushed, false if the circular buffer is full.
  bool pushBottom(Task<void> *task) noexcept;

  /// @brief Pops a task from the bottom of the deque (called only by the owning
  /// thread).
  /// @return The popped task, or nullptr if empty or lost race to a thief.
  Task<void> *popBottom() noexcept;

  /// @brief Steals a task from the top of the deque (called concurrently by
  /// thief threads).
  /// @return The stolen task, or nullptr if empty or CAS failed.
  Task<void> *stealTop() noexcept;

  /// @brief Returns the approximate number of tasks currently in the deque.
  [[nodiscard]] usize size() const noexcept;

  /// @brief Returns true if the deque is currently empty.
  [[nodiscard]] bool isEmpty() const noexcept;

  /// @brief Returns total capacity of the ring buffer.
  [[nodiscard]] usize getCapacity() const noexcept { return m_capacity; }

  [[nodiscard]] i64 getTop() const noexcept {
    return m_top.load(std::memory_order_relaxed);
  }
  [[nodiscard]] i64 getBottom() const noexcept {
    return m_bottom.load(std::memory_order_relaxed);
  }

private:
  alignas(64) std::atomic<i64> m_top{0};
  alignas(64) std::atomic<i64> m_bottom{0};
  alignas(64) Task<void> **m_buffer{nullptr};
  usize m_capacity{0};
  usize m_mask{0};
  bool m_ownsBuffer{true};
};

} // namespace engine::core
