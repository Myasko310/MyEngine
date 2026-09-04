#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

class Entity
{
public:
    Entity() = default;

    Entity(uint32_t id, const std::string& name = "")
        : m_ID(id), m_Name(name)
    {
    }

    uint32_t GetID() const
    {
        return m_ID;
    }

    const std::string& GetName() const
    {
        return m_Name;
    }

    void SetName(const std::string& name)
    {
        m_Name = name;
    }

    // --- Tag & Layer ---
    const std::string& GetTag() const   { return m_Tag; }
    void SetTag(const std::string& tag) { m_Tag = tag; }

    // Layer is a single index [0-31] identifying which layer this entity
    // belongs to.  Use LayerMask::GetName/SetName to manage layer names.
    uint32_t GetLayer() const          { return m_Layer; }
    void     SetLayer(uint32_t layer)  { m_Layer = layer; }

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        T& reference = *component;

        m_Components[std::type_index(typeid(T))] = component;

        return reference;
    }

    template<typename T>
    bool HasComponent() const
    {
        return m_Components.find(std::type_index(typeid(T))) != m_Components.end();
    }

    template<typename T>
    T& GetComponent()
    {
        return *std::static_pointer_cast<T>(
            m_Components.at(std::type_index(typeid(T)))
        );
    }

    template<typename T>
    const T& GetComponent() const
    {
        return *std::static_pointer_cast<T>(
            m_Components.at(std::type_index(typeid(T)))
        );
    }

    template<typename T>
    void RemoveComponent()
    {
        m_Components.erase(std::type_index(typeid(T)));
    }

private:
    uint32_t    m_ID    = 0;
    std::string m_Name;
    std::string m_Tag;
    uint32_t    m_Layer = 0;  // index into LayerMask registry

    std::unordered_map<std::type_index, std::shared_ptr<void>> m_Components;
};