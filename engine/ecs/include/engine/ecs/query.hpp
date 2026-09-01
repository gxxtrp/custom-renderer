#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/ecs/archetype.hpp>
#include <engine/ecs/entity.hpp>

namespace engine::ecs {

template <typename... Ts> class QueryView {
public:
  explicit QueryView(std::vector<Archetype *> matchingArchetypes)
      : m_matchingArchetypes(std::move(matchingArchetypes)) {}

  [[nodiscard]] core::usize count() const noexcept {
    core::usize total = 0;
    for (const auto *arch : m_matchingArchetypes) {
      if (arch != nullptr) {
        total += arch->size();
      }
    }
    return total;
  }

  [[nodiscard]] bool empty() const noexcept { return count() == 0; }

  template <typename Func> void each(Func &&func) const {
    for (auto *arch : m_matchingArchetypes) {
      if (arch == nullptr || arch->empty()) {
        continue;
      }

      const auto entities = arch->getEntities();
      auto columns = std::make_tuple(
          arch->template getColumn<std::remove_reference_t<Ts>>()...);

      const core::usize numEntities = entities.size();
      for (core::usize i = 0; i < numEntities; ++i) {
        if constexpr (std::is_invocable_v<Func, Entity, Ts &...>) {
          func(entities[i],
               std::get<std::span<std::remove_reference_t<Ts>>>(columns)[i]...);
        } else if constexpr (std::is_invocable_v<Func, Ts &...>) {
          func(std::get<std::span<std::remove_reference_t<Ts>>>(columns)[i]...);
        }
      }
    }
  }

  [[nodiscard]] const std::vector<Archetype *> &
  getMatchingArchetypes() const noexcept {
    return m_matchingArchetypes;
  }

private:
  std::vector<Archetype *> m_matchingArchetypes;
};

} // namespace engine::ecs
