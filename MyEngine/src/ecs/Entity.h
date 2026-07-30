#pragma once
#include <cstdint>

namespace MyEngine {

    // An entity is just a unique ID
    using EntityID = uint32_t;
    const EntityID INVALID_ENTITY = 0;

    class Entity {
    public:
        Entity() : m_ID(0) {}
        Entity(EntityID id) : m_ID(id) {}

        EntityID GetID() const { return m_ID; }
        bool IsValid() const { return m_ID != INVALID_ENTITY; }

        bool operator==(const Entity& other) const { return m_ID == other.m_ID; }
        bool operator!=(const Entity& other) const { return m_ID != other.m_ID; }

    private:
        EntityID m_ID;
    };

} // namespace MyEngine