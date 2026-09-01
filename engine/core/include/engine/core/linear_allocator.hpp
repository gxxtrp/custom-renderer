#pragma once

#include <cstddef>
#include <new>
#include <span>
#include <utility>

#include <engine/core/types.hpp>

namespace engine::core {

class LinearAllocator {
public:
    explicit LinearAllocator(usize capacity);
    ~LinearAllocator();

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    LinearAllocator(LinearAllocator&& other) noexcept;
    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

    void* allocate(usize size, usize alignment = alignof(std::max_align_t));

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* const memory = allocate(sizeof(T), alignof(T));
        if (!memory) {
            return nullptr;
        }
        return ::new (memory) T(std::forward<Args>(args)...);
    }

    template<typename T>
    T* allocateArray(usize count, usize alignment = alignof(T)) {
        if (count == 0) {
            return nullptr;
        }
        void* const memory = allocate(sizeof(T) * count, alignment);
        return static_cast<T*>(memory);
    }

    template<typename T>
    std::span<T> allocateSpan(usize count, usize alignment = alignof(T)) {
        T* const arrayPtr = allocateArray<T>(count, alignment);
        if (!arrayPtr) {
            return {};
        }
        return std::span<T>(arrayPtr, count);
    }

    void reset() noexcept;

    [[nodiscard]] usize getAllocatedBytes() const noexcept { return m_offset; }
    [[nodiscard]] usize getCapacity() const noexcept { return m_capacity; }
    [[nodiscard]] u8* getBaseAddress() const noexcept { return m_memory; }

private:
    u8* m_memory{nullptr};
    usize m_capacity{0};
    usize m_offset{0};
};

}  // namespace engine::core
