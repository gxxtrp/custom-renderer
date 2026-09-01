#pragma once

#include <compare>

#include <engine/core/types.hpp>

namespace engine::ecs {

struct Entity {
  core::u32 id{core::u32(~0u)};
  core::u32 generation{0};

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return id != core::u32(~0u);
  }

  [[nodiscard]] constexpr auto operator<=>(const Entity &) const = default;
};

inline constexpr Entity NullEntity{core::u32(~0u), 0};

} // namespace engine::ecs
