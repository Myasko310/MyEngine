#pragma once
#include "Entity.h"
#include "Component.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

namespace MyEngine {

    class Scene {
    public:
        Scene();
        ~Scene();

        // Create a new entity
        Entity CreateEntity();

        // Destroy an entity and all its components
        void DestroyEntity(Entity entity);

        // Add a component to an entity
        template<typename T, typename... Args>
        T& AddComponent(Entity entity, Args&&... args) {
            auto comp = std::make_shared<T>(std::forward<Args>(args)...);
            comp->ownerID = entity.GetID();
            m_Components[std::type_index(typeid(T))][entity.GetID()] = comp;
            return *comp;
        }

        // Get a component from an entity
        template<typename T>
        T* GetComponent(Entity entity) {
            auto typeIt = m_Components.find(std::type_index(typeid(T)));
            if (typeIt == m_Components.end()) return nullptr;

            auto entIt = typeIt->second.find(entity.GetID());
            if (entIt == typeIt->second.end()) return nullptr;

            return static_cast<T*>(entIt->second.get());
        }

        // Check if entity has a component
        template<typename T>
        bool HasComponent(Entity entity) {
            return GetComponent<T>(entity) != nullptr;
        }

        // Remove a component from an entity
        template<typename T>
        void RemoveComponent(Entity entity) {
            auto typeIt = m_Components.find(std::type_index(typeid(T)));
            if (typeIt != m_Components.end()) {
                typeIt->second.erase(entity.GetID());
            }
        }

        // Get all entities
        const std::vector<Entity>& GetEntities() const { return m_Entities; }

    private:
        std::vector<Entity> m_Entities;
        EntityID m_NextID = 1;

        // [ComponentType][EntityID] -> Component
        std::unordered_map<
            std::type_index,
            std::unordered_map<EntityID, std::shared_ptr<Component>>
        > m_Components;
    };

} // namespace MyEngine