#pragma once

#include "ecs/Component.h"
#include <glm/glm.hpp>

namespace MyEngine
{
	enum class MovingPartType
	{
		Platform = 0,
		Door = 1,
		Elevator = 2
	};

	struct MovingPlatformComponent : public Component
	{
		MovingPartType type = MovingPartType::Platform;
		bool active = true;
		bool pingPong = true;
		bool autoReturn = true;

		glm::vec3 startPosition = glm::vec3(0.0f);
		glm::vec3 endPosition = glm::vec3(0.0f, 3.0f, 0.0f);
		float speed = 2.0f;
		float waitTime = 0.5f;

		float currentLerp = 0.0f;
		float waitTimer = 0.0f;
		int direction = 1;
		bool initialized = false;
		glm::vec3 lastPosition = glm::vec3(0.0f);
		glm::vec3 velocity = glm::vec3(0.0f);
	};
}
