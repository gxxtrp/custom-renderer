#pragma once

#include <engine/core/types.hpp>

namespace engine::rhi {

struct ColorClearValue {
    core::f32 r{0.0f};
    core::f32 g{0.0f};
    core::f32 b{0.0f};
    core::f32 a{1.0f};
};

struct DepthStencilClearValue {
    core::f32 depth{1.0f};
    core::u32 stencil{0};
};

struct Extent2D {
    core::u32 width{0};
    core::u32 height{0};
};

struct Extent3D {
    core::u32 width{0};
    core::u32 height{0};
    core::u32 depth{1};
};

struct Offset2D {
    core::i32 x{0};
    core::i32 y{0};
};

struct Offset3D {
    core::i32 x{0};
    core::i32 y{0};
    core::i32 z{0};
};

struct Rect2D {
    Offset2D offset{};
    Extent2D extent{};
};

struct Viewport {
    core::f32 x{0.0f};
    core::f32 y{0.0f};
    core::f32 width{0.0f};
    core::f32 height{0.0f};
    core::f32 minDepth{0.0f};
    core::f32 maxDepth{1.0f};
};

}  // namespace engine::rhi
