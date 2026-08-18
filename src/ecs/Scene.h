#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ecs/Entity.h"

class Scene
{
public:
    Scene() = default;

    std::shared_ptr<Entity> CreateEntity(const std::string& name = "")
    {
        auto entity = std::make_shared<Entity>(m_NextEntityID++, name);
        m_Entities.push_back(entity);
        return entity;
    }

    void DestroyEntity(uint32_t entityID)
    {
        m_Entities.erase(
            std::remove_if(
                m_Entities.begin(),
                m_Entities.end(),
                [entityID](const std::shared_ptr<Entity>& entity)
                {
                    return entity && entity->GetID() == entityID;
                }
            ),
            m_Entities.end()
        );
    }

    const std::vector<std::shared_ptr<Entity>>& GetEntities() const
    {
        return m_Entities;
    }

    std::vector<std::shared_ptr<Entity>>& GetEntities()
    {
        return m_Entities;
    }

    // --- Tag & Layer queries ---

    // Return the first entity with the given tag, or nullptr.
    Entity* FindEntityByTag(const std::string& tag) const
    {
        for (auto& e : m_Entities)
            if (e && e->GetTag() == tag)
                return e.get();
        return nullptr;
    }

    // Return all entities with the given tag.
    std::vector<Entity*> FindEntitiesByTag(const std::string& tag) const
    {
        std::vector<Entity*> result;
        for (auto& e : m_Entities)
            if (e && e->GetTag() == tag)
                result.push_back(e.get());
        return result;
    }

    // Return all entities whose layer index equals `layer`.
    std::vector<Entity*> FindEntitiesByLayer(uint32_t layer) const
    {
        std::vector<Entity*> result;
        for (auto& e : m_Entities)
            if (e && e->GetLayer() == layer)
                result.push_back(e.get());
        return result;
    }

    // Return all entities whose layer is set in `layerMask` (bitmask).
    std::vector<Entity*> FindEntitiesByLayerMask(uint32_t layerMask) const
    {
        std::vector<Entity*> result;
        for (auto& e : m_Entities)
            if (e && ((1u << e->GetLayer()) & layerMask))
                result.push_back(e.get());
        return result;
    }

private:
    uint32_t m_NextEntityID = 1;
    std::vector<std::shared_ptr<Entity>> m_Entities;

};