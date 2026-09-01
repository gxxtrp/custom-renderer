#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/ecs/component.hpp>
#include <engine/ecs/entity.hpp>

namespace engine::ecs {

class Archetype {
public:
  explicit Archetype(std::vector<ComponentInfo> componentInfos);
  ~Archetype();

  Archetype(const Archetype &) = delete;
  Archetype &operator=(const Archetype &) = delete;

  Archetype(Archetype &&) noexcept = default;
  Archetype &operator=(Archetype &&) noexcept = default;

  [[nodiscard]] std::span<const Entity> getEntities() const noexcept {
    return m_entities;
  }

  [[nodiscard]] core::usize size() const noexcept { return m_entities.size(); }
  [[nodiscard]] bool empty() const noexcept { return m_entities.empty(); }

  [[nodiscard]] const ComponentTypeMask &getMask() const noexcept {
    return m_mask;
  }

  [[nodiscard]] bool hasComponent(ComponentTypeId id) const noexcept {
    if (id < MaxComponentTypes) {
      return m_mask.test(id);
    }
    return false;
  }

  template <typename T> [[nodiscard]] bool hasComponent() const noexcept {
    return hasComponent(getComponentTypeId<T>());
  }

  [[nodiscard]] const std::vector<ComponentInfo> &
  getComponentInfos() const noexcept {
    return m_componentInfos;
  }

  [[nodiscard]] core::i32 getColumnIndex(ComponentTypeId id) const noexcept;

  template <typename T> [[nodiscard]] std::span<T> getColumn() noexcept {
    const core::i32 colIdx = getColumnIndex(getComponentTypeId<T>());
    if (colIdx < 0 || m_entities.empty()) {
      return {};
    }
    return std::span<T>(reinterpret_cast<T *>(
                            m_columns[static_cast<core::usize>(colIdx)].data()),
                        m_entities.size());
  }

  template <typename T>
  [[nodiscard]] std::span<const T> getColumn() const noexcept {
    const core::i32 colIdx = getColumnIndex(getComponentTypeId<T>());
    if (colIdx < 0 || m_entities.empty()) {
      return {};
    }
    return std::span<const T>(
        reinterpret_cast<const T *>(
            m_columns[static_cast<core::usize>(colIdx)].data()),
        m_entities.size());
  }

  [[nodiscard]] void *getRawComponent(core::usize row,
                                      ComponentTypeId typeId) noexcept;
  [[nodiscard]] const void *
  getRawComponent(core::usize row, ComponentTypeId typeId) const noexcept;

  template <typename T>
  [[nodiscard]] T &getComponent(core::usize row) noexcept {
    assert(row < m_entities.size() && "Archetype row out of range");
    void *ptr = getRawComponent(row, getComponentTypeId<T>());
    assert(ptr != nullptr && "Component not present in archetype");
    return *reinterpret_cast<T *>(ptr);
  }

  template <typename T>
  [[nodiscard]] const T &getComponent(core::usize row) const noexcept {
    assert(row < m_entities.size() && "Archetype row out of range");
    const void *ptr = getRawComponent(row, getComponentTypeId<T>());
    assert(ptr != nullptr && "Component not present in archetype");
    return *reinterpret_cast<const T *>(ptr);
  }

  core::u32 addEntity(Entity entity);
  std::optional<Entity> removeEntity(core::usize row);

  void setAddEdge(ComponentTypeId typeId, Archetype *target) noexcept {
    m_addEdges[typeId] = target;
  }

  [[nodiscard]] Archetype *getAddEdge(ComponentTypeId typeId) const noexcept {
    auto it = m_addEdges.find(typeId);
    return it != m_addEdges.end() ? it->second : nullptr;
  }

  void setRemoveEdge(ComponentTypeId typeId, Archetype *target) noexcept {
    m_removeEdges[typeId] = target;
  }

  [[nodiscard]] Archetype *
  getRemoveEdge(ComponentTypeId typeId) const noexcept {
    auto it = m_removeEdges.find(typeId);
    return it != m_removeEdges.end() ? it->second : nullptr;
  }

private:
  ComponentTypeMask m_mask{};
  std::vector<ComponentInfo> m_componentInfos;
  std::vector<core::i32> m_typeToColumn;
  std::vector<Entity> m_entities;
  std::vector<std::vector<core::u8>> m_columns;

  std::unordered_map<ComponentTypeId, Archetype *> m_addEdges;
  std::unordered_map<ComponentTypeId, Archetype *> m_removeEdges;
};

} // namespace engine::ecs
