#include <cstdlib>
#include <utility>

#include <engine/core/linear_allocator.hpp>

namespace engine::core {

LinearAllocator::LinearAllocator(usize capacity) : m_capacity(capacity) {
    if (m_capacity > 0) {
        m_memory = static_cast<u8*>(std::malloc(m_capacity));
    }
}

LinearAllocator::~LinearAllocator() {
    if (m_memory) {
        std::free(m_memory);
        m_memory = nullptr;
    }
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : m_memory(std::exchange(other.m_memory, nullptr)), m_capacity(std::exchange(other.m_capacity, 0)),
      m_offset(std::exchange(other.m_offset, 0)) {}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
    if (this != &other) {
        if (m_memory) {
            std::free(m_memory);
        }
        m_memory = std::exchange(other.m_memory, nullptr);
        m_capacity = std::exchange(other.m_capacity, 0);
        m_offset = std::exchange(other.m_offset, 0);
    }
    return *this;
}

void* LinearAllocator::allocate(usize size, usize alignment) {
    if (size == 0 || !m_memory) {
        return nullptr;
    }

    if (alignment == 0) {
        alignment = alignof(std::max_align_t);
    }

    const auto currentAddress = reinterpret_cast<std::uintptr_t>(m_memory + m_offset);
    const std::uintptr_t mask = alignment - 1;
    const std::uintptr_t alignedAddress = (currentAddress + mask) & ~mask;
    const auto padding = static_cast<usize>(alignedAddress - currentAddress);

    if (m_offset + padding + size > m_capacity) {
        return nullptr;
    }

    m_offset += padding + size;
    return reinterpret_cast<void*>(alignedAddress);
}

void LinearAllocator::reset() noexcept {
    m_offset = 0;
}

}  // namespace engine::core
