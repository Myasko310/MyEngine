#include "network/ServerNetworkSystem.h"
#include <algorithm>
#include <iostream>

namespace MyEngine::Net
{
	ServerNetworkSystem::ServerNetworkSystem(std::shared_ptr<NetworkManager> manager)
		: m_NetworkManager(manager)
	{
		if (!m_NetworkManager || !m_NetworkManager->IsServer())
		{
			std::cerr << "ServerNetworkSystem requires a server-mode NetworkManager" << std::endl;
			return;
		}

		// Register callback for receiving client input
		m_NetworkManager->SetInputCommandCallback(
			[this](uint32_t clientID, const InputCommand& input)
			{
				// Validate and process input
				bool isValid = true;
				if (m_ValidateInputCallback)
					isValid = m_ValidateInputCallback(clientID, input);

				if (isValid)
				{
					m_State.latestClientInputs[clientID] = input;
					if (m_ProcessInputCallback)
						m_ProcessInputCallback(clientID, input);
				}
			}
		);
	}

	void ServerNetworkSystem::RegisterEntityForReplication(uint32_t entityID)
	{
		m_RegisteredEntities[entityID] = true;
	}

	void ServerNetworkSystem::UnregisterEntityForReplication(uint32_t entityID)
	{
		m_RegisteredEntities.erase(entityID);
		m_State.authorizedEntityStates.erase(entityID);
	}

	void ServerNetworkSystem::UpdateEntityState(const EntityState& state)
	{
		if (m_RegisteredEntities.find(state.entityID) == m_RegisteredEntities.end())
			return; // Entity not registered for replication

		m_State.authorizedEntityStates[state.entityID] = state;
	}

	void ServerNetworkSystem::Update()
	{
		if (!m_NetworkManager || !m_NetworkManager->IsServer())
			return;

		// Update connected clients list from NetworkManager
		m_ConnectedClients = m_NetworkManager->GetConnectedClientIDs();

		// Process incoming client inputs
		ProcessClientInputs();

		// Broadcast updated entity states to all clients
		BroadcastState();
	}

	bool ServerNetworkSystem::IsRunning() const
	{
		return m_NetworkManager && m_NetworkManager->IsServer() && m_NetworkManager->IsConnected();
	}

	const std::vector<uint32_t>& ServerNetworkSystem::GetConnectedClients() const
	{
		return m_ConnectedClients;
	}

	void ServerNetworkSystem::SetValidateInputCallback(ValidateInputCallback callback)
	{
		m_ValidateInputCallback = callback;
	}

	void ServerNetworkSystem::SetProcessInputCallback(ProcessInputCallback callback)
	{
		m_ProcessInputCallback = callback;
	}

	void ServerNetworkSystem::ProcessClientInputs()
	{
		// Inputs are already being processed via the callback registered in constructor
		// This method is here for any additional server-side input validation/logging
	}

	void ServerNetworkSystem::BroadcastState()
	{
		if (!m_NetworkManager || m_ConnectedClients.empty())
			return;

		// Broadcast all registered entity states to all clients
		for (const auto& [entityID, state] : m_State.authorizedEntityStates)
		{
			m_NetworkManager->BroadcastEntityState(state);
		}
	}
}
