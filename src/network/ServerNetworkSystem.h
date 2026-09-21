#pragma once

#include "network/NetworkMessages.h"
#include "network/NetworkManager.h"
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

namespace MyEngine::Net
{
	// ============================================================
	// ServerNetworkState
	// ============================================================
	// Tracks server-side connection state and client information
	struct ServerNetworkState
	{
		std::unordered_map<uint32_t, ConnectionInfo> clients;
		std::unordered_map<uint32_t, InputCommand> latestClientInputs;
		std::unordered_map<uint32_t, EntityState> authorizedEntityStates;
	};

	// ============================================================
	// ServerNetworkSystem
	// ============================================================
	// Authoritative server-side integration. Accepts client
	// connections, validates input, manages entity state,
	// and broadcasts state to all connected clients.
	// ============================================================
	class ServerNetworkSystem
	{
	public:
		explicit ServerNetworkSystem(std::shared_ptr<NetworkManager> manager);
		~ServerNetworkSystem() = default;

		// Register an entity's state for replication
		void RegisterEntityForReplication(uint32_t entityID);
		void UnregisterEntityForReplication(uint32_t entityID);

		// Update entity state (will be broadcast to clients)
		void UpdateEntityState(const EntityState& state);

		// Call once per frame to process client input and broadcast state
		void Update();

		// Check server status
		bool IsRunning() const;
		const std::vector<uint32_t>& GetConnectedClients() const;

		// Callbacks for validating/processing client input
		using ValidateInputCallback = std::function<bool(uint32_t clientID, const InputCommand&)>;
		using ProcessInputCallback = std::function<void(uint32_t clientID, const InputCommand&)>;

		void SetValidateInputCallback(ValidateInputCallback callback);
		void SetProcessInputCallback(ProcessInputCallback callback);

	private:
		std::shared_ptr<NetworkManager> m_NetworkManager;
		ServerNetworkState m_State;
		std::vector<uint32_t> m_ConnectedClients;
		std::unordered_map<uint32_t, bool> m_RegisteredEntities;

		ValidateInputCallback m_ValidateInputCallback;
		ProcessInputCallback m_ProcessInputCallback;

		void ProcessClientInputs();
		void BroadcastState();
	};
}
