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

private:
    uint32_t m_NextEntityID = 1;
    std::vector<std::shared_ptr<Entity>> m_Entities;
};