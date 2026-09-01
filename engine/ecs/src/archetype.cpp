#include <engine/ecs/archetype.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

namespace engine::ecs {

Archetype::Archetype(std::vector<ComponentInfo> componentInfos)
    : m_componentInfos(std::move(componentInfos)) {
  std::sort(m_componentInfos.begin(), m_componentInfos.end(),
            [](const ComponentInfo &a, const ComponentInfo &b) {
              return a.id < b.id;
            });

  m_typeToColumn.assign(MaxComponentTypes, -1);
  m_columns.resize(m_componentInfos.size());

  for (core::usize i = 0; i < m_componentInfos.size(); ++i) {
    const auto id = m_componentInfos[i].id;
    m_mask.set(id);
    if (id < m_typeToColumn.size()) {
      m_typeToColumn[id] = static_cast<core::i32>(i);
    }
  }
}

Archetype::~Archetype() {
  for (core::usize row = 0; row < m_entities.size(); ++row) {
    for (core::usize i = 0; i < m_componentInfos.size(); ++i) {
      const auto &info = m_componentInfos[i];
      if (info.destruct != nullptr) {
        void *ptr = m_columns[i].data() + (row * info.size);
        info.destruct(ptr);
      }
    }
  }
}

core::i32 Archetype::getColumnIndex(ComponentTypeId id) const noexcept {
  if (id < m_typeToColumn.size()) {
    return m_typeToColumn[id];
  }
  return -1;
}

void *Archetype::getRawComponent(core::usize row,
                                 ComponentTypeId typeId) noexcept {
  const core::i32 colIdx = getColumnIndex(typeId);
  if (colIdx < 0 || row >= m_entities.size()) {
    return nullptr;
  }
  const auto &info = m_componentInfos[static_cast<core::usize>(colIdx)];
  return m_columns[static_cast<core::usize>(colIdx)].data() + (row * info.size);
}

const void *Archetype::getRawComponent(core::usize row,
                                       ComponentTypeId typeId) const noexcept {
  const core::i32 colIdx = getColumnIndex(typeId);
  if (colIdx < 0 || row >= m_entities.size()) {
    return nullptr;
  }
  const auto &info = m_componentInfos[static_cast<core::usize>(colIdx)];
  return m_columns[static_cast<core::usize>(colIdx)].data() + (row * info.size);
}

core::u32 Archetype::addEntity(Entity entity) {
  const auto row = static_cast<core::u32>(m_entities.size());
  m_entities.push_back(entity);

  for (core::usize i = 0; i < m_componentInfos.size(); ++i) {
    const auto &info = m_componentInfos[i];
    auto &col = m_columns[i];
    const auto oldSize = col.size();
    col.resize(oldSize + info.size);
    void *ptr = col.data() + oldSize;
    if (info.construct != nullptr) {
      info.construct(ptr);
    }
  }

  return row;
}

std::optional<Entity> Archetype::removeEntity(core::usize row) {
  assert(row < m_entities.size() && "Row index out of range for removeEntity");

  const core::usize lastRow = m_entities.size() - 1;
  if (row == lastRow) {
    for (core::usize i = 0; i < m_componentInfos.size(); ++i) {
      const auto &info = m_componentInfos[i];
      auto &col = m_columns[i];
      void *ptr = col.data() + (lastRow * info.size);
      if (info.destruct != nullptr) {
        info.destruct(ptr);
      }
      col.resize(lastRow * info.size);
    }
    m_entities.pop_back();
    return std::nullopt;
  }

  // Swap-and-pop with last row
  const Entity swappedEntity = m_entities[lastRow];

  for (core::usize i = 0; i < m_componentInfos.size(); ++i) {
    const auto &info = m_componentInfos[i];
    auto &col = m_columns[i];
    void *dst = col.data() + (row * info.size);
    void *src = col.data() + (lastRow * info.size);

    if (info.destruct != nullptr) {
      info.destruct(dst);
    }
    if (info.moveConstruct != nullptr) {
      info.moveConstruct(dst, src);
      if (info.destruct != nullptr) {
        info.destruct(src);
      }
    } else {
      std::memcpy(dst, src, info.size);
    }

    col.resize(lastRow * info.size);
  }

  m_entities[row] = swappedEntity;
  m_entities.pop_back();

  return swappedEntity;
}

} // namespace engine::ecs
