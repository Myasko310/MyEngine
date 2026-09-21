#pragma once

#include "network/NetworkMessages.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>

namespace MyEngine::Net
{
	// Forward declarations
	class ClientConnection;
	class ServerConnection;

	// ============================================================
	// NetworkManager
	// ============================================================
	// Central hub for networking. Manages either client-side or
	// server-side connections, packet routing, and state replication.
	// Uses socket-based transport for local network communication.
	//
	// Usage (Client):
	//   auto manager = NetworkManager::CreateClient("127.0.0.1", 5000);
	//   manager->Update(); // Call each frame
	//   if (manager->IsConnected()) { ... }
	//
	// Usage (Server):
	//   auto manager = NetworkManager::CreateServer(5000, maxClients);
	//   manager->Update();
	//   for (auto& client : manager->GetConnectedClients()) { ... }
	// ============================================================
	class NetworkManager
	{
	public:
		~NetworkManager();

		// Initialization
		static std::shared_ptr<NetworkManager> CreateClient(const std::string& serverAddress, uint16_t serverPort);
		static std::shared_ptr<NetworkManager> CreateServer(uint16_t listenPort, int maxConnections = 32);

		// Update logic (call once per frame)
		void Update();

		// Connection status
		bool IsConnected() const { return m_IsConnected; }
		bool IsServer() const { return m_IsServer; }
		uint32_t GetLocalClientID() const { return m_LocalClientID; }

		// Client-side: send input to server
		void SendInputCommand(const InputCommand& input);

		// Server-side: broadcast state to all clients
		void BroadcastEntityState(const EntityState& state);

		// Server-side: get connected clients
		const std::vector<uint32_t>& GetConnectedClientIDs() const;

		// Callbacks for incoming messages
		using InputCommandCallback = std::function<void(uint32_t clientID, const InputCommand&)>;
		using EntityStateCallback = std::function<void(const EntityState&)>;

		void SetInputCommandCallback(InputCommandCallback callback);
		void SetEntityStateCallback(EntityStateCallback callback);

		// Diagnostics
		uint32_t GetPacketsSentThisFrame() const { return m_PacketsSentThisFrame; }
		uint32_t GetPacketsReceivedThisFrame() const { return m_PacketsReceivedThisFrame; }

		// Public constructor for factory methods
		NetworkManager(bool isServer, uint32_t clientID);

	private:

		bool m_IsServer = false;
		bool m_IsConnected = false;
		uint32_t m_LocalClientID = 0;

		// Client-side
		std::unique_ptr<ClientConnection> m_ClientConnection;

		// Server-side
		std::unique_ptr<ServerConnection> m_ServerConnection;
		std::vector<uint32_t> m_ConnectedClientIDs;

		// Callbacks
		InputCommandCallback m_InputCommandCallback;
		EntityStateCallback m_EntityStateCallback;

		// Diagnostics
		uint32_t m_PacketsSentThisFrame = 0;
		uint32_t m_PacketsReceivedThisFrame = 0;

		// Internal methods
		void ProcessClientMessages();
		void ProcessServerMessages();
	};
}
