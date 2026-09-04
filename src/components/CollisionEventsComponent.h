#pragma once

#include <functional>
#include <memory>

class Entity;

// Attach this component to an entity to receive collision/trigger
// enter and exit callbacks from PhysicsSystem. Each callback receives
// the *other* entity involved in the collision/overlap.
//
// - onCollisionEnter/Exit fire for physical collisions (neither collider
//   involved has isTrigger set).
// - onTriggerEnter/Exit fire when at least one of the colliders involved
//   has isTrigger set to true (no physical response is applied in that case).
struct CollisionEventsComponent
{
	std::function<void(const std::shared_ptr<Entity>&)> onCollisionEnter;
	std::function<void(const std::shared_ptr<Entity>&)> onCollisionExit;
	std::function<void(const std::shared_ptr<Entity>&)> onTriggerEnter;
	std::function<void(const std::shared_ptr<Entity>&)> onTriggerExit;

	// ScriptSystem can attach its own handlers here without overwriting any
	// existing editor/gameplay C++ callbacks above.
	std::function<void(const std::shared_ptr<Entity>&)> onScriptCollisionEnter;
	std::function<void(const std::shared_ptr<Entity>&)> onScriptCollisionExit;
	std::function<void(const std::shared_ptr<Entity>&)> onScriptTriggerEnter;
	std::function<void(const std::shared_ptr<Entity>&)> onScriptTriggerExit;
};
