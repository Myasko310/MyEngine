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
		bool sprintPressed = false;
		bool slidePressed = false;
		bool crouchHeld = false;
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

	struct TerrainPatchPointDelta
	{
		std::int32_t row = 0;
		std::int32_t col = 0;
		float heightBefore = 0.0f;
		float heightAfter = 0.0f;
	};

	struct TerrainPatchDelta
	{
		std::uint32_t terrainEntityID = 0;
		std::int32_t resolution = 0;
		std::int32_t minRow = 0;
		std::int32_t maxRow = 0;
		std::int32_t minCol = 0;
		std::int32_t maxCol = 0;
		NetTick authoredTick = 0;
		std::vector<TerrainPatchPointDelta> points;
	};

	struct WorldSnapshot
	{
		NetTick tick = 0;
		std::vector<ReplicatedEntityState> entities;
		std::vector<TerrainPatchDelta> terrainPatches;
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
