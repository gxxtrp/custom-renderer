#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include <engine/core/types.hpp>

namespace engine::core {

template <typename T = void> class alignas(64) Task;

/// @brief Move-only 64-byte task representation for tasks returning void.
/// Fits completely within a single 64-byte cache line with inline capture
/// storage.
template <> class alignas(64) Task<void> {
public:
  using ExecuteFn = void (*)(void *storage) noexcept;
  using DestroyFn = void (*)(void *storage) noexcept;
  using MoveFn = void (*)(void *dest, void *src) noexcept;

  static constexpr usize inlineStorageSize = 40;
  static constexpr usize inlineAlignment = 8;

  constexpr Task() noexcept = default;

  ~Task() { reset(); }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  Task(Task &&other) noexcept { moveFrom(std::move(other)); }

  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(std::move(other));
    }
    return *this;
  }

  template <typename F>
    requires(!std::is_same_v<std::decay_t<F>, Task> &&
             std::is_invocable_v<std::decay_t<F>>)
  explicit Task(F &&callable) {
    using DecayedF = std::decay_t<F>;
    static_assert(sizeof(DecayedF) <= inlineStorageSize,
                  "Callable capture size exceeds Task 40-byte inline storage");
    static_assert(alignof(DecayedF) <= inlineAlignment,
                  "Callable alignment exceeds Task 8-byte alignment");

    ::new (static_cast<void *>(m_storage)) DecayedF(std::forward<F>(callable));

    m_execute = [](void *storage) noexcept {
      (*reinterpret_cast<DecayedF *>(storage))();
    };

    m_destroy = [](void *storage) noexcept {
      reinterpret_cast<DecayedF *>(storage)->~DecayedF();
    };

    m_move = [](void *dest, void *src) noexcept {
      ::new (dest) DecayedF(std::move(*reinterpret_cast<DecayedF *>(src)));
      reinterpret_cast<DecayedF *>(src)->~DecayedF();
    };
  }

  void execute() noexcept {
    if (m_execute) {
      m_execute(m_storage);
    }
  }

  void reset() noexcept {
    if (m_destroy) {
      m_destroy(m_storage);
      m_destroy = nullptr;
    }
    m_execute = nullptr;
    m_move = nullptr;
  }

  [[nodiscard]] bool isValid() const noexcept { return m_execute != nullptr; }

  explicit operator bool() const noexcept { return isValid(); }

private:
  void moveFrom(Task &&other) noexcept {
    m_execute = other.m_execute;
    m_destroy = other.m_destroy;
    m_move = other.m_move;

    if (other.m_move) {
      other.m_move(m_storage, other.m_storage);
    }

    other.m_execute = nullptr;
    other.m_destroy = nullptr;
    other.m_move = nullptr;
  }

  ExecuteFn m_execute{nullptr};                        // 8 bytes
  DestroyFn m_destroy{nullptr};                        // 8 bytes
  MoveFn m_move{nullptr};                              // 8 bytes
  alignas(8) std::byte m_storage[inlineStorageSize]{}; // 40 bytes
};
static_assert(sizeof(Task<void>) == 64,
              "Task<void> must occupy exactly 64 bytes (1 cache line)");
static_assert(alignof(Task<void>) == 64,
              "Task<void> must be 64-byte cache line aligned");

/// @brief Move-only 64-byte task representation for tasks returning a typed
/// value T. Fits completely within a single 64-byte cache line with inline
/// capture storage.
template <typename T> class alignas(64) Task {
public:
  using ExecuteFn = void (*)(void *storage, void *resultDest) noexcept;
  using DestroyFn = void (*)(void *storage) noexcept;
  using MoveFn = void (*)(void *dest, void *src) noexcept;

  static constexpr usize inlineStorageSize = 32;
  static constexpr usize inlineAlignment = 8;

  constexpr Task() noexcept = default;

  ~Task() { reset(); }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  Task(Task &&other) noexcept { moveFrom(std::move(other)); }

  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(std::move(other));
    }
    return *this;
  }

  template <typename F>
    requires(!std::is_same_v<std::decay_t<F>, Task> &&
             std::is_invocable_r_v<T, std::decay_t<F>>)
  explicit Task(F &&callable, void *resultDest = nullptr)
      : m_resultDest(resultDest) {
    using DecayedF = std::decay_t<F>;
    static_assert(
        sizeof(DecayedF) <= inlineStorageSize,
        "Callable capture size exceeds Task<T> 32-byte inline storage");
    static_assert(alignof(DecayedF) <= inlineAlignment,
                  "Callable alignment exceeds Task<T> 8-byte alignment");

    ::new (static_cast<void *>(m_storage)) DecayedF(std::forward<F>(callable));

    m_execute = [](void *storage, void *resultDestPtr) noexcept {
      auto &fn = *reinterpret_cast<DecayedF *>(storage);
      if (resultDestPtr) {
        ::new (resultDestPtr) T(fn());
      } else {
        (void)fn();
      }
    };

    m_destroy = [](void *storage) noexcept {
      reinterpret_cast<DecayedF *>(storage)->~DecayedF();
    };

    m_move = [](void *dest, void *src) noexcept {
      ::new (dest) DecayedF(std::move(*reinterpret_cast<DecayedF *>(src)));
      reinterpret_cast<DecayedF *>(src)->~DecayedF();
    };
  }

  void setResultDestination(void *resultDest) noexcept {
    m_resultDest = resultDest;
  }

  void execute() noexcept {
    if (m_execute) {
      m_execute(m_storage, m_resultDest);
    }
  }

  void reset() noexcept {
    if (m_destroy) {
      m_destroy(m_storage);
      m_destroy = nullptr;
    }
    m_execute = nullptr;
    m_move = nullptr;
    m_resultDest = nullptr;
  }

  [[nodiscard]] bool isValid() const noexcept { return m_execute != nullptr; }

  explicit operator bool() const noexcept { return isValid(); }

private:
  void moveFrom(Task &&other) noexcept {
    m_execute = other.m_execute;
    m_destroy = other.m_destroy;
    m_move = other.m_move;
    m_resultDest = other.m_resultDest;

    if (other.m_move) {
      other.m_move(m_storage, other.m_storage);
    }

    other.m_execute = nullptr;
    other.m_destroy = nullptr;
    other.m_move = nullptr;
    other.m_resultDest = nullptr;
  }

  ExecuteFn m_execute{nullptr};                        // 8 bytes
  DestroyFn m_destroy{nullptr};                        // 8 bytes
  MoveFn m_move{nullptr};                              // 8 bytes
  void *m_resultDest{nullptr};                         // 8 bytes
  alignas(8) std::byte m_storage[inlineStorageSize]{}; // 32 bytes
};
static_assert(sizeof(Task<int>) == 64,
              "Task<T> must occupy exactly 64 bytes (1 cache line)");
static_assert(alignof(Task<int>) == 64,
              "Task<T> must be 64-byte cache line aligned");

// Deduction guides for Task
template <typename F> Task(F &&) -> Task<std::invoke_result_t<std::decay_t<F>>>;

Task() -> Task<void>;

using TaskVoid = Task<void>;

} // namespace engine::core
