#pragma once

#include <span>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

enum class PipelineBindPoint : core::u32 { Graphics = 0, Compute };

enum class CullMode : core::u32 { None = 0, Front, Back, FrontAndBack };

enum class FrontFace : core::u32 { CounterClockwise = 0, Clockwise };

enum class PolygonMode : core::u32 { Fill = 0, Line, Point };

enum class CompareOp : core::u32 {
  Never = 0,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always
};

struct RasterizerState {
  PolygonMode polygonMode{PolygonMode::Fill};
  CullMode cullMode{CullMode::Back};
  FrontFace frontFace{FrontFace::CounterClockwise};
  core::f32 lineWidth{1.0f};
  bool depthClampEnable{false};
};

struct DepthStencilState {
  CompareOp depthCompareOp{CompareOp::LessOrEqual};
  bool depthTestEnable{true};
  bool depthWriteEnable{true};
};

struct ColorBlendAttachment {
  bool blendEnable{false};
  Format format{Format::RGBA8_UNORM};
};

struct PipelineDesc {
  std::span<const core::u8> taskShaderCode{};
  std::span<const core::u8> meshShaderCode{};
  std::span<const core::u8> fragmentShaderCode{};
  std::span<const core::u8> computeShaderCode{};

  std::span<const Format> colorAttachmentFormats{};
  Format depthAttachmentFormat{Format::Undefined};

  RasterizerState rasterizer{};
  DepthStencilState depthStencil{};

  PipelineBindPoint bindPoint{PipelineBindPoint::Graphics};
};

class Pipeline {
public:
  virtual ~Pipeline() = default;

  Pipeline(const Pipeline &) = delete;
  Pipeline &operator=(const Pipeline &) = delete;

  Pipeline(Pipeline &&) noexcept = default;
  Pipeline &operator=(Pipeline &&) noexcept = default;

  [[nodiscard]] virtual PipelineBindPoint getBindPoint() const noexcept = 0;

protected:
  Pipeline() = default;
};

} // namespace engine::rhi
