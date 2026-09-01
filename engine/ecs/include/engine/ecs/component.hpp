#pragma once

#include <bitset>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <engine/core/types.hpp>

namespace engine::ecs {

using ComponentTypeId = core::u32;
constexpr core::usize MaxComponentTypes = 64;
using ComponentTypeMask = std::bitset<MaxComponentTypes>;

inline ComponentTypeId getNextComponentTypeId() noexcept {
  static ComponentTypeId nextId = 0;
  return nextId++;
}

template <typename T>
[[nodiscard]] inline ComponentTypeId getComponentTypeId() noexcept {
  static const ComponentTypeId id = getNextComponentTypeId();
  return id;
}

using ComponentConstructorFn = void (*)(void *dst);
using ComponentDestructorFn = void (*)(void *ptr);
using ComponentMoveConstructorFn = void (*)(void *dst, void *src);

struct ComponentInfo {
  ComponentTypeId id{0};
  core::usize size{0};
  core::usize alignment{0};
  std::string_view name{};
  ComponentConstructorFn construct{nullptr};
  ComponentDestructorFn destruct{nullptr};
  ComponentMoveConstructorFn moveConstruct{nullptr};

  template <typename T>
  [[nodiscard]] static ComponentInfo
  create(std::string_view name = "") noexcept {
    return ComponentInfo{
        .id = getComponentTypeId<T>(),
        .size = sizeof(T),
        .alignment = alignof(T),
        .name = name,
        .construct =
            [](void *dst) {
              if constexpr (std::is_default_constructible_v<T>) {
                new (dst) T();
              }
            },
        .destruct =
            [](void *ptr) {
              if constexpr (!std::is_trivially_destructible_v<T>) {
                static_cast<T *>(ptr)->~T();
              }
            },
        .moveConstruct =
            [](void *dst, void *src) {
              new (dst) T(std::move(*static_cast<T *>(src)));
            }};
  }
};

template <typename... Ts>
[[nodiscard]] constexpr ComponentTypeMask makeComponentMask() noexcept {
  ComponentTypeMask mask;
  (mask.set(getComponentTypeId<Ts>()), ...);
  return mask;
}

} // namespace engine::ecs
