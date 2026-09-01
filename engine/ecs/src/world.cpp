#include <engine/ecs/world.hpp>

namespace engine::ecs {

World::World() {
  m_entityRecords.reserve(1024);
  m_freeEntityIds.reserve(256);
  m_archetypes.reserve(64);

  m_emptyArchetype = getOrCreateArchetype({});
}

World::~World() = default;

Entity World::createEntity() {
  if (!m_freeEntityIds.empty()) {
    const auto id = m_freeEntityIds.back();
    m_freeEntityIds.pop_back();

    auto &record = m_entityRecords[id];
    record.alive = true;
    record.archetype = m_emptyArchetype;

    const Entity entity{id, record.generation};
    record.row = m_emptyArchetype->addEntity(entity);
    return entity;
  }

  const auto id = static_cast<core::u32>(m_entityRecords.size());
  const Entity entity{id, 0};
  const auto row = m_emptyArchetype->addEntity(entity);

  m_entityRecords.push_back(EntityRecord{
      .archetype = m_emptyArchetype,
      .row = row,
      .generation = 0,
      .alive = true,
  });

  return entity;
}

void World::destroyEntity(Entity entity) {
  if (!isAlive(entity)) {
    return;
  }

  auto &record = m_entityRecords[entity.id];
  auto swappedEntity = record.archetype->removeEntity(record.row);
  if (swappedEntity.has_value()) {
    m_entityRecords[swappedEntity->id].row = record.row;
  }

  record.alive = false;
  record.archetype = nullptr;
  record.row = 0;
  ++record.generation;

  m_freeEntityIds.push_back(entity.id);
}

bool World::isAlive(Entity entity) const noexcept {
  return entity.id < m_entityRecords.size() &&
         m_entityRecords[entity.id].alive &&
         m_entityRecords[entity.id].generation == entity.generation;
}

Archetype *World::getOrCreateArchetype(std::vector<ComponentInfo> infos) {
  ComponentTypeMask mask;
  for (const auto &info : infos) {
    mask.set(info.id);
  }

  auto it = m_archetypeMap.find(mask);
  if (it != m_archetypeMap.end()) {
    return it->second;
  }

  auto newArch = std::make_unique<Archetype>(std::move(infos));
  auto *ptr = newArch.get();
  m_archetypeMap[mask] = ptr;
  m_archetypes.push_back(std::move(newArch));
  return ptr;
}

} // namespace engine::ecs
