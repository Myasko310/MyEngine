#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MyEngine::Net
{
	using NetTick = std::uint32_t;
	using ClientID = std::uint32_t;

	struct InputCommand
	{
		NetTick tick = 0;
		glm::vec2 moveAxis = glm::vec2(0.0f);
		bool jumpPressed = false;
	};

	struct ReplicatedEntityState
	{
		std::uint32_t entityID = 0;
		glm::vec3 position = glm::vec3(0.0f);
		glm::vec3 rotation = glm::vec3(0.0f);
		glm::vec3 velocity = glm::vec3(0.0f);
		bool isGrounded = false;
		bool animationPlaying = false;
		bool audioPlaying = false;
		int activeAnimationClipIndex = -1;
		float animationTimeSeconds = 0.0f;
		std::string audioEventName;
	};

	struct WorldSnapshot
	{
		NetTick tick = 0;
		std::vector<ReplicatedEntityState> entities;
	};

	struct InputMessage
	{
		ClientID clientID = 0;
		InputCommand command;
	};

	enum class SessionControlType
	{
		ConnectRequest,
		ConnectAccept,
		Disconnect
	};

	struct SessionControlMessage
	{
		SessionControlType type = SessionControlType::ConnectRequest;
		ClientID requestedClientID = 0;
		ClientID assignedClientID = 0;
		NetTick serverTick = 0;
	};

	struct SnapshotMessage
	{
		ClientID clientID = 0;
		WorldSnapshot snapshot;
	};
}
