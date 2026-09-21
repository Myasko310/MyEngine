#include "network/ClientNetworkSystem.h"
#include <algorithm>

namespace MyEngine::Net
{
	ClientNetworkSystem::ClientNetworkSystem(std::shared_ptr<NetworkManager> manager)
		: m_NetworkManager(manager)
	{
		if (!m_NetworkManager)
			return;

		m_State.isConnected = m_NetworkManager->IsConnected();
		m_State.clientID = m_NetworkManager->GetLocalClientID();

		// Register callback for remote entity updates
		m_NetworkManager->SetEntityStateCallback(
			[this](const EntityState& state)
			{
				m_State.remoteEntityStates[state.entityID] = state;
				if (m_EntityUpdateCallback)
					m_EntityUpdateCallback(state);
			}
		);
	}

	void ClientNetworkSystem::QueuePlayerInput(const InputCommand& input)
	{
		if (!m_State.isConnected || !m_NetworkManager)
			return;

		InputCommand cmd = input;
		cmd.clientID = m_State.clientID;
		cmd.commandID = m_State.nextCommandID++;

		m_State.pendingInputs.push(cmd);
	}

	void ClientNetworkSystem::Update()
	{
		if (!m_NetworkManager)
			return;

		m_State.isConnected = m_NetworkManager->IsConnected();

		// Send all queued inputs to server
		while (!m_State.pendingInputs.empty())
		{
			const InputCommand& input = m_State.pendingInputs.front();
			m_NetworkManager->SendInputCommand(input);
			m_State.pendingInputs.pop();
		}

		// Process remote updates
		ProcessRemoteUpdates();
	}

	bool ClientNetworkSystem::IsConnected() const
	{
		return m_State.isConnected && m_NetworkManager && m_NetworkManager->IsConnected();
	}

	uint32_t ClientNetworkSystem::GetClientID() const
	{
		return m_State.clientID;
	}

	void ClientNetworkSystem::SetRemoteEntityUpdateCallback(RemoteEntityUpdateCallback callback)
	{
		m_EntityUpdateCallback = callback;
	}

	void ClientNetworkSystem::ProcessRemoteUpdates()
	{
		// The NetworkManager already processes updates via callbacks,
		// so this is a placeholder for future client-side extrapolation
		// or interpolation logic
	}
}
