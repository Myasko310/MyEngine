#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MyEngine::Editor
{
	// Editor-only view state: never written to a scene CameraComponent.
	struct CharacterPreviewCamera
	{
		glm::vec3 target = glm::vec3(0.0f);
		float radius = 1.0f;
		float yaw = 0.0f;
		float pitch = 10.0f;
		float distance = 4.0f;
		float verticalOffset = 0.0f;
		static constexpr float fieldOfView = 45.0f;

		void Focus(const glm::vec3& minimum, const glm::vec3& maximum, float aspect)
		{
			target = (minimum + maximum) * 0.5f;
			radius = std::max(glm::length(maximum - minimum) * 0.5f, 0.05f);
			verticalOffset = 0.0f;
			const float halfVertical = glm::radians(fieldOfView * 0.5f);
			const float halfHorizontal = std::atan(std::tan(halfVertical) * std::max(aspect, 0.01f));
			distance = radius * 1.25f / std::sin(std::min(halfVertical, halfHorizontal));
		}

		void Orbit(float yawDelta, float pitchDelta)
		{
			yaw = std::remainder(yaw + yawDelta, 360.0f);
			pitch = std::clamp(pitch + pitchDelta, -85.0f, 85.0f);
		}

		void Zoom(float steps)
		{
			distance = std::clamp(distance * std::exp(-std::clamp(steps, -20.0f, 20.0f) * 0.15f),
				radius * 0.2f, radius * 200.0f);
		}

		glm::vec3 GetTarget() const { return target + glm::vec3(0.0f, verticalOffset, 0.0f); }

		glm::vec3 GetPosition() const
		{
			const float y = glm::radians(yaw);
			const float p = glm::radians(pitch);
			return GetTarget() + distance * glm::vec3(std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p));
		}

		glm::mat4 GetView() const
		{
			return glm::lookAt(GetPosition(), GetTarget(), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		glm::mat4 GetProjection(float aspect) const
		{
			return glm::perspective(glm::radians(fieldOfView), std::max(aspect, 0.01f),
				radius * 0.001f, std::max(distance + radius * 20.0f, radius * 200.0f));
		}
	};
}
