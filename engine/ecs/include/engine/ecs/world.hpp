#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/ecs/archetype.hpp>
#include <engine/ecs/component.hpp>
#include <engine/ecs/entity.hpp>
#include <engine/ecs/query.hpp>

namespace engine::ecs {

struct EntityRecord {
  Archetype *archetype{nullptr};
  core::u32 row{0};
  core::u32 generation{0};
  bool alive{false};
};

class World {
public:
  World();
  ~World();

  World(const World &) = delete;
  World &operator=(const World &) = delete;

  World(World &&) noexcept = default;
  World &operator=(World &&) noexcept = default;

  Entity createEntity();
  void destroyEntity(Entity entity);
  [[nodiscard]] bool isAlive(Entity entity) const noexcept;

  template <typename T> void addComponent(Entity entity, T &&component) {
    assert(isAlive(entity) && "Entity is not alive");
    auto &record = m_entityRecords[entity.id];
    auto *curArch = record.archetype;
    const auto typeId = getComponentTypeId<std::decay_t<T>>();

    if (curArch->hasComponent(typeId)) {
      curArch->template getComponent<std::decay_t<T>>(record.row) =
          std::forward<T>(component);
      return;
    }

    auto *targetArch = curArch->getAddEdge(typeId);
    if (targetArch == nullptr) {
      std::vector<ComponentInfo> newInfos = curArch->getComponentInfos();
      newInfos.push_back(ComponentInfo::create<std::decay_t<T>>());
      targetArch = getOrCreateArchetype(std::move(newInfos));
      curArch->setAddEdge(typeId, targetArch);
      targetArch->setRemoveEdge(typeId, curArch);
    }

    const auto newRow = targetArch->addEntity(entity);

    // Move existing components from curArch to targetArch
    for (const auto &info : curArch->getComponentInfos()) {
      void *dst = targetArch->getRawComponent(newRow, info.id);
      void *src = curArch->getRawComponent(record.row, info.id);
      if (info.destruct != nullptr) {
        info.destruct(dst);
      }
      if (info.moveConstruct != nullptr) {
        info.moveConstruct(dst, src);
      } else {
        std::memcpy(dst, src, info.size);
      }
    }

    // Set new component
    void *newCompDst = targetArch->getRawComponent(newRow, typeId);
    using ValueType = std::decay_t<T>;
    static_cast<ValueType *>(newCompDst)->~ValueType();
    new (newCompDst) ValueType(std::forward<T>(component));

    // Remove from old archetype
    auto swappedEntity = curArch->removeEntity(record.row);
    if (swappedEntity.has_value()) {
      m_entityRecords[swappedEntity->id].row = record.row;
    }

    record.archetype = targetArch;
    record.row = newRow;
  }

  template <typename T> void removeComponent(Entity entity) {
    assert(isAlive(entity) && "Entity is not alive");
    auto &record = m_entityRecords[entity.id];
    auto *curArch = record.archetype;
    const auto typeId = getComponentTypeId<T>();

    if (!curArch->hasComponent(typeId)) {
      return;
    }

    auto *targetArch = curArch->getRemoveEdge(typeId);
    if (targetArch == nullptr) {
      std::vector<ComponentInfo> newInfos;
      for (const auto &info : curArch->getComponentInfos()) {
        if (info.id != typeId) {
          newInfos.push_back(info);
        }
      }
      targetArch = getOrCreateArchetype(std::move(newInfos));
      curArch->setRemoveEdge(typeId, targetArch);
      targetArch->setAddEdge(typeId, curArch);
    }

    const auto newRow = targetArch->addEntity(entity);

    // Move matching components to targetArch
    for (const auto &info : targetArch->getComponentInfos()) {
      void *dst = targetArch->getRawComponent(newRow, info.id);
      void *src = curArch->getRawComponent(record.row, info.id);
      if (info.destruct != nullptr) {
        info.destruct(dst);
      }
      if (info.moveConstruct != nullptr) {
        info.moveConstruct(dst, src);
      } else {
        std::memcpy(dst, src, info.size);
      }
    }

    // Remove from old archetype
    auto swappedEntity = curArch->removeEntity(record.row);
    if (swappedEntity.has_value()) {
      m_entityRecords[swappedEntity->id].row = record.row;
    }

    record.archetype = targetArch;
    record.row = newRow;
  }

  template <typename T> [[nodiscard]] bool has(Entity entity) const noexcept {
    if (!isAlive(entity)) {
      return false;
    }
    return m_entityRecords[entity.id].archetype->template hasComponent<T>();
  }

  template <typename T> [[nodiscard]] T &get(Entity entity) noexcept {
    assert(isAlive(entity) && "Entity is not alive");
    return m_entityRecords[entity.id].archetype->template getComponent<T>(
        m_entityRecords[entity.id].row);
  }

  template <typename T>
  [[nodiscard]] const T &get(Entity entity) const noexcept {
    assert(isAlive(entity) && "Entity is not alive");
    return m_entityRecords[entity.id].archetype->template getComponent<T>(
        m_entityRecords[entity.id].row);
  }

  template <typename... Ts> [[nodiscard]] QueryView<Ts...> view() {
    std::vector<Archetype *> matching;
    const ComponentTypeMask requiredMask =
        makeComponentMask<std::remove_reference_t<Ts>...>();

    for (const auto &archPtr : m_archetypes) {
      if ((archPtr->getMask() & requiredMask) == requiredMask) {
        matching.push_back(archPtr.get());
      }
    }

    return QueryView<Ts...>(std::move(matching));
  }

  [[nodiscard]] core::usize getEntityCount() const noexcept {
    return m_entityRecords.size() - m_freeEntityIds.size();
  }

  Archetype *getOrCreateArchetype(std::vector<ComponentInfo> infos);

private:
  std::vector<EntityRecord> m_entityRecords;
  std::vector<core::u32> m_freeEntityIds;
  std::vector<std::unique_ptr<Archetype>> m_archetypes;
  std::unordered_map<ComponentTypeMask, Archetype *> m_archetypeMap;
  Archetype *m_emptyArchetype{nullptr};
};

} // namespace engine::ecs
