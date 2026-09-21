#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace MyEngine::Net
{
	// ============================================================
	// Network Message Types
	// ============================================================
	enum class MessageType : uint8_t
	{
		// Client -> Server
		ClientInput = 1,
		ClientConnect = 2,
		ClientDisconnect = 3,

		// Server -> Client
		ServerStateUpdate = 10,
		ServerAcceptConnection = 11,
		ServerRejectConnection = 12,
		ServerEntitySpawned = 13,
		ServerEntityDestroyed = 14,
	};

	// ============================================================
	// Input Command (replicated from client to server)
	// ============================================================
	struct InputCommand
	{
		uint32_t clientID = 0;
		uint32_t commandID = 0;                  // Incrementing counter for ordering
		uint32_t targetEntityID = 0;              // Entity this input applies to
		glm::vec3 moveDirection = glm::vec3(0.0f);
		float moveSpeed = 0.0f;
		uint32_t inputFlags = 0;                  // Bitmask: jump, attack, etc.

		static const uint32_t INPUT_FLAG_JUMP = 1 << 0;
		static const uint32_t INPUT_FLAG_SPRINT = 1 << 1;
		static const uint32_t INPUT_FLAG_ATTACK1 = 1 << 2;
		static const uint32_t INPUT_FLAG_ATTACK2 = 1 << 3;
		static const uint32_t INPUT_FLAG_ATTACK3 = 1 << 4;
		static const uint32_t INPUT_FLAG_INTERACT = 1 << 5;
	};

	// ============================================================
	// Entity State (replicated from server to clients)
	// ============================================================
	struct EntityState
	{
		uint32_t entityID = 0;
		glm::vec3 position = glm::vec3(0.0f);
		glm::vec3 rotation = glm::vec3(0.0f);
		uint32_t animationStateHash = 0;         // Animation state for this entity
		uint32_t health = 100;
		bool isAlive = true;
	};

	// ============================================================
	// Network Packet Structure
	// ============================================================
	struct NetworkPacket
	{
		MessageType messageType;
		uint8_t data[1024];
		size_t dataSize = 0;

		NetworkPacket() = default;
		explicit NetworkPacket(MessageType type) : messageType(type), dataSize(0) {}
	};

	// ============================================================
	// Connection Info
	// ============================================================
	struct ConnectionInfo
	{
		uint32_t clientID = 0;
		bool isConnected = false;
		bool isServer = false;                   // True if this is server-side
		uint32_t latencyMs = 0;
	};
}
