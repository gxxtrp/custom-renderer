#include <engine/core/work_stealing_deque.hpp>

#include <bit>
#include <cassert>
#include <new>

#include <engine/core/linear_allocator.hpp>

namespace engine::core {

WorkStealingDeque::WorkStealingDeque(usize capacity)
    : m_capacity(capacity), m_mask(capacity - 1), m_ownsBuffer(true) {
  assert(std::has_single_bit(capacity) &&
         "WorkStealingDeque capacity must be a power of 2");
  m_buffer = new Task<void> *[capacity];
  for (usize i = 0; i < capacity; ++i) {
    m_buffer[i] = nullptr;
  }
}

WorkStealingDeque::WorkStealingDeque(usize capacity, LinearAllocator &allocator)
    : m_capacity(capacity), m_mask(capacity - 1), m_ownsBuffer(false) {
  assert(std::has_single_bit(capacity) &&
         "WorkStealingDeque capacity must be a power of 2");
  m_buffer =
      allocator.allocateArray<Task<void> *>(capacity, alignof(Task<void> *));
  assert(m_buffer != nullptr &&
         "Failed to allocate WorkStealingDeque buffer from LinearAllocator");
  for (usize i = 0; i < capacity; ++i) {
    m_buffer[i] = nullptr;
  }
}

WorkStealingDeque::~WorkStealingDeque() {
  if (m_ownsBuffer) {
    delete[] m_buffer;
  }
  m_buffer = nullptr;
}

bool WorkStealingDeque::pushBottom(Task<void> *task) noexcept {
  const i64 b = m_bottom.load(std::memory_order_relaxed);
  const i64 t = m_top.load(std::memory_order_acquire);

  if (b - t >= static_cast<i64>(m_capacity)) {
    return false;
  }

  m_buffer[static_cast<usize>(b) & m_mask] = task;
  m_bottom.store(b + 1, std::memory_order_release);
  return true;
}

Task<void> *WorkStealingDeque::popBottom() noexcept {
  const i64 b = m_bottom.load(std::memory_order_relaxed) - 1;
  m_bottom.store(b, std::memory_order_seq_cst);
  i64 t = m_top.load(std::memory_order_seq_cst);

  if (t <= b) {
    Task<void> *task = m_buffer[static_cast<usize>(b) & m_mask];
    if (t == b) {
      // Last element: race against concurrent thief attempting stealTop
      if (!m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                         std::memory_order_relaxed)) {
        task = nullptr;
      }
      m_bottom.store(b + 1, std::memory_order_relaxed);
    }
    return task;
  }

  // Deque was empty
  m_bottom.store(b + 1, std::memory_order_relaxed);
  return nullptr;
}

Task<void> *WorkStealingDeque::stealTop() noexcept {
  i64 t = m_top.load(std::memory_order_seq_cst);
  const i64 b = m_bottom.load(std::memory_order_seq_cst);

  if (t < b) {
    Task<void> *task = m_buffer[static_cast<usize>(t) & m_mask];
    if (m_top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst,
                                      std::memory_order_relaxed)) {
      return task;
    }
  }
  return nullptr;
}

usize WorkStealingDeque::size() const noexcept {
  const i64 b = m_bottom.load(std::memory_order_relaxed);
  const i64 t = m_top.load(std::memory_order_relaxed);
  return (b > t) ? static_cast<usize>(b - t) : 0;
}

bool WorkStealingDeque::isEmpty() const noexcept {
  const i64 b = m_bottom.load(std::memory_order_relaxed);
  const i64 t = m_top.load(std::memory_order_relaxed);
  return b <= t;
}

} // namespace engine::core
