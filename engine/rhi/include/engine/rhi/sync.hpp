#pragma once

#include <cstdint>

#include <engine/core/types.hpp>

namespace engine::rhi {

struct FenceDesc {
    bool signaled{false};
};

class Fence {
public:
    virtual ~Fence() = default;

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    Fence(Fence&&) noexcept = default;
    Fence& operator=(Fence&&) noexcept = default;

    virtual void wait(core::u64 timeoutNs = UINT64_MAX) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual bool isSignaled() const noexcept = 0;

protected:
    Fence() = default;
};

struct SemaphoreDesc {
    core::u64 initialValue{0};
    bool timeline{false};
};

class Semaphore {
public:
    virtual ~Semaphore() = default;

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    Semaphore(Semaphore&&) noexcept = default;
    Semaphore& operator=(Semaphore&&) noexcept = default;

    [[nodiscard]] virtual bool isTimeline() const noexcept = 0;
    [[nodiscard]] virtual core::u64 getValue() const = 0;
    virtual void wait(core::u64 value, core::u64 timeoutNs = UINT64_MAX) = 0;
    virtual void signal(core::u64 value) = 0;

protected:
    Semaphore() = default;
};

}  // namespace engine::rhi
