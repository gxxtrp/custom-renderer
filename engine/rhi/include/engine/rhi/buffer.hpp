#pragma once

#include <expected>
#include <string>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

struct BufferDesc {
    core::usize size{0};
    BufferUsageFlags usage{BufferUsageFlags::None};
    MemoryUsage memoryUsage{MemoryUsage::GpuOnly};
};

class Buffer {
public:
    virtual ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    [[nodiscard]] virtual core::usize getSize() const noexcept = 0;
    [[nodiscard]] virtual BufferUsageFlags getUsage() const noexcept = 0;
    [[nodiscard]] virtual MemoryUsage getMemoryUsage() const noexcept = 0;

    virtual std::expected<void*, std::string> map() = 0;
    virtual void unmap() = 0;

    [[nodiscard]] virtual core::u64 getDeviceAddress() const noexcept = 0;

protected:
    Buffer() = default;
};

}  // namespace engine::rhi
