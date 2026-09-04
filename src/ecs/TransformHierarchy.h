#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "components/TransformComponent.h"

// Helpers for resolving parent-child transform hierarchies.
// Parenting is stored as TransformComponent::parentID (0 = root).
namespace TransformHierarchy
{
	inline std::shared_ptr<Entity> FindEntityByID(const Scene& scene, uint32_t id)
	{
		if (id == 0)
			return nullptr;
		for (const auto& entity : scene.GetEntities())
		{
			if (entity && entity->GetID() == id)
				return entity;
		}
		return nullptr;
	}

	// Returns the world matrix for an entity, walking up the parent chain.
	// Cycles are guarded by a depth limit.
	inline glm::mat4 GetWorldMatrix(const Scene& scene, const Entity& entity)
	{
		if (!entity.HasComponent<TransformComponent>())
			return glm::mat4(1.0f);

		glm::mat4 matrix = entity.GetComponent<TransformComponent>().GetMatrix();

		uint32_t parentID = entity.GetComponent<TransformComponent>().parentID;
		int depth = 0;
		while (parentID != 0 && depth++ < 64)
		{
			auto parent = FindEntityByID(scene, parentID);
			if (!parent || !parent->HasComponent<TransformComponent>())
				break;

			const auto& parentTransform = parent->GetComponent<TransformComponent>();
			matrix = parentTransform.GetMatrix() * matrix;
			parentID = parentTransform.parentID;
		}

		return matrix;
	}

	// True if 'potentialAncestor' is anywhere in 'entity's parent chain
	// (used to prevent reparenting cycles).
	inline bool IsAncestorOf(const Scene& scene, uint32_t potentialAncestorID, const Entity& entity)
	{
		if (!entity.HasComponent<TransformComponent>())
			return false;

		uint32_t parentID = entity.GetComponent<TransformComponent>().parentID;
		int depth = 0;
		while (parentID != 0 && depth++ < 64)
		{
			if (parentID == potentialAncestorID)
				return true;
			auto parent = FindEntityByID(scene, parentID);
			if (!parent || !parent->HasComponent<TransformComponent>())
				break;
			parentID = parent->GetComponent<TransformComponent>().parentID;
		}
		return false;
	}

	// Reparents 'entity' under 'newParentID' (0 to unparent), preserving the
	// entity's current world position by converting it into the new parent's
	// local space. Returns false if the change would create a cycle.
	inline bool SetParent(const Scene& scene, Entity& entity, uint32_t newParentID)
	{
		if (!entity.HasComponent<TransformComponent>())
			return false;
		if (newParentID == entity.GetID())
			return false;
		if (newParentID != 0)
		{
			auto newParent = FindEntityByID(scene, newParentID);
			if (!newParent)
				return false;
			// Prevent cycles: the new parent must not be a descendant of this entity.
			if (IsAncestorOf(scene, entity.GetID(), *newParent))
				return false;
		}

		auto& transform = entity.GetComponent<TransformComponent>();

		// Preserve world position: convert current world matrix into new parent space.
		glm::mat4 world = GetWorldMatrix(scene, entity);
		glm::mat4 newParentWorld(1.0f);
		if (newParentID != 0)
		{
			auto newParent = FindEntityByID(scene, newParentID);
			newParentWorld = GetWorldMatrix(scene, *newParent);
		}
		glm::mat4 newLocal = glm::inverse(newParentWorld) * world;

		// Decompose only translation/scale naively; full decomposition of rotation
		// uses the upper-left 3x3. This is adequate for TRS matrices without shear.
		transform.position = glm::vec3(newLocal[3]);
		transform.scale = glm::vec3(
			glm::length(glm::vec3(newLocal[0])),
			glm::length(glm::vec3(newLocal[1])),
			glm::length(glm::vec3(newLocal[2])));
		glm::mat3 rotationMatrix(
			glm::vec3(newLocal[0]) / (transform.scale.x != 0.0f ? transform.scale.x : 1.0f),
			glm::vec3(newLocal[1]) / (transform.scale.y != 0.0f ? transform.scale.y : 1.0f),
			glm::vec3(newLocal[2]) / (transform.scale.z != 0.0f ? transform.scale.z : 1.0f));
		transform.rotation = glm::eulerAngles(glm::quat_cast(rotationMatrix));

		transform.parentID = newParentID;
		return true;
	}
}
