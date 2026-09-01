#pragma once

#include <span>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

enum class PipelineBindPoint : core::u8 { Graphics = 0, Compute };

enum class CullMode : core::u8 { None = 0, Front, Back, FrontAndBack };

enum class FrontFace : core::u8 { CounterClockwise = 0, Clockwise };

enum class PolygonMode : core::u8 { Fill = 0, Line, Point };

enum class CompareOp : core::u8 { Never = 0, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always };

struct RasterizerState {
    core::f32 lineWidth{1.0f};
    PolygonMode polygonMode{PolygonMode::Fill};
    CullMode cullMode{CullMode::Back};
    FrontFace frontFace{FrontFace::CounterClockwise};
    bool depthClampEnable{false};
};

struct DepthStencilState {
    CompareOp depthCompareOp{CompareOp::LessOrEqual};
    bool depthTestEnable{true};
    bool depthWriteEnable{true};
};

struct ColorBlendAttachment {
    Format format{Format::RGBA8_UNORM};
    bool blendEnable{false};
};

struct PipelineDesc {
    std::span<const core::u8> taskShaderCode{};
    std::span<const core::u8> meshShaderCode{};
    std::span<const core::u8> fragmentShaderCode{};
    std::span<const core::u8> computeShaderCode{};
    std::span<const Format> colorAttachmentFormats{};

    RasterizerState rasterizer{};
    DepthStencilState depthStencil{};
    Format depthAttachmentFormat{Format::Undefined};
    PipelineBindPoint bindPoint{PipelineBindPoint::Graphics};
};

class Pipeline {
public:
    virtual ~Pipeline() = default;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(Pipeline&&) noexcept = default;

    [[nodiscard]] virtual PipelineBindPoint getBindPoint() const noexcept = 0;

protected:
    Pipeline() = default;
};

}  // namespace engine::rhi
