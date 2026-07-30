#include "Scene.h"
#include <algorithm>

namespace MyEngine {

    Scene::Scene() {}
    Scene::~Scene() {}

    Entity Scene::CreateEntity() {
        Entity entity(m_NextID++);
        m_Entities.push_back(entity);
        printf("[Scene] Created Entity ID: %d\n", entity.GetID());
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        // Remove all components
        for (auto& [type, entityMap] : m_Components) {
            entityMap.erase(entity.GetID());
        }

        // Remove from entity list
        m_Entities.erase(
            std::remove(m_Entities.begin(), m_Entities.end(), entity),
            m_Entities.end()
        );

        printf("[Scene] Destroyed Entity ID: %d\n", entity.GetID());
    }

} // namespace MyEngine