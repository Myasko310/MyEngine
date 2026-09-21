#pragma once

#include "network/NetworkMessages.h"
#include "network/NetworkManager.h"
#include <memory>
#include <functional>
#include <queue>
#include <unordered_map>

namespace MyEngine::Net
{
	// ============================================================
	// ClientNetworkState
	// ============================================================
	// Tracks the local client's connection state and input queue
	struct ClientNetworkState
	{
		uint32_t clientID = 0;
		bool isConnected = false;
		uint32_t nextCommandID = 0;
		std::queue<InputCommand> pendingInputs;

		// Remote state cache for extrapolation
		std::unordered_map<uint32_t, EntityState> remoteEntityStates;
	};

	// ============================================================
	// ClientNetworkSystem
	// ============================================================
	// Simplified client-side integration point for sending player
	// input and receiving entity state updates from the server
	// ============================================================
	class ClientNetworkSystem
	{
	public:
		explicit ClientNetworkSystem(std::shared_ptr<NetworkManager> manager);
		~ClientNetworkSystem() = default;

		// Queue local player input for sending to server
		void QueuePlayerInput(const InputCommand& input);

		// Call once per frame to send queued input and receive updates
		void Update();

		// Check connection status
		bool IsConnected() const;
		uint32_t GetClientID() const;

		// Callbacks
		using RemoteEntityUpdateCallback = std::function<void(const EntityState&)>;
		void SetRemoteEntityUpdateCallback(RemoteEntityUpdateCallback callback);

	private:
		std::shared_ptr<NetworkManager> m_NetworkManager;
		ClientNetworkState m_State;
		RemoteEntityUpdateCallback m_EntityUpdateCallback;

		void ProcessRemoteUpdates();
	};
}
