#include "network/NetTransport.h"

#include <algorithm>
#include <random>

namespace
{
	template <typename T>
	bool ShouldDropPacket(const MyEngine::Net::NetworkSimulationSettings& settings, std::mt19937& rng)
	{
		if (!settings.enabled || settings.packetLossChance <= 0.0f)
			return false;
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return dist(rng) < std::clamp(settings.packetLossChance, 0.0f, 1.0f);
	}

	MyEngine::Net::NetTick ComputeJitterTicks(const MyEngine::Net::NetworkSimulationSettings& settings, std::mt19937& rng)
	{
		if (!settings.enabled)
			return 0;
		const MyEngine::Net::NetTick lo = std::min(settings.jitterMinTicks, settings.jitterMaxTicks);
		const MyEngine::Net::NetTick hi = std::max(settings.jitterMinTicks, settings.jitterMaxTicks);
		if (hi == 0)
			return 0;
		std::uniform_int_distribution<MyEngine::Net::NetTick> dist(lo, hi);
		return dist(rng);
	}

	template <typename T>
	void MaybeReorderTail(std::deque<T>& delayedQueue, const MyEngine::Net::NetworkSimulationSettings& settings, std::mt19937& rng)
	{
		if (!settings.enabled || delayedQueue.size() < 2 || settings.reorderChance <= 0.0f)
			return;
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		if (dist(rng) < std::clamp(settings.reorderChance, 0.0f, 1.0f))
			std::iter_swap(delayedQueue.end() - 1, delayedQueue.end() - 2);
	}
}

namespace MyEngine::Net
{
	void InMemoryTransport::SetSimulationSettings(const NetworkSimulationSettings& settings)
	{
		m_SimulationSettings = settings;
	}

	const NetworkSimulationSettings& InMemoryTransport::GetSimulationSettings() const
	{
		return m_SimulationSettings;
	}

	void InMemoryTransport::BeginServerSession()
	{
		m_NextClientID = 1u;
		m_AssignedClientID = 0u;
		m_SessionReady = false;
		m_SessionTimedOut = false;
		m_LastRequestedClientID = 0u;
		m_LastConnectRequestTick = 0u;
		m_LastRetryTick = 0u;
		m_ConnectRetryCount = 0u;
		while (!m_ConnectRequestsToServer.empty())
			m_ConnectRequestsToServer.pop();
		while (!m_SessionControlToClient.empty())
			m_SessionControlToClient.pop();
	}

	void InMemoryTransport::SetSessionRetrySettings(const SessionRetrySettings& settings)
	{
		m_SessionRetrySettings = settings;
	}

	const SessionRetrySettings& InMemoryTransport::GetSessionRetrySettings() const
	{
		return m_SessionRetrySettings;
	}

	void InMemoryTransport::SendConnectRequest(ClientID requestedClientID)
	{
		SessionControlMessage message;
		message.type = SessionControlType::ConnectRequest;
		message.requestedClientID = requestedClientID;
		message.serverTick = m_CurrentTick;
		m_ConnectRequestsToServer.push(message);
		m_LastRequestedClientID = requestedClientID;
		m_LastConnectRequestTick = m_CurrentTick;
		m_LastRetryTick = m_CurrentTick;
		m_SessionTimedOut = false;
		m_HasOutstandingConnect = true;
	}

	void InMemoryTransport::DisconnectSession()
	{
		SessionControlMessage message;
		message.type = SessionControlType::Disconnect;
		message.assignedClientID = m_AssignedClientID;
		message.serverTick = m_CurrentTick;
		m_SessionControlToClient.push(message);
		m_AssignedClientID = 0u;
		m_SessionReady = false;
		m_HasOutstandingConnect = false;
	}

	void InMemoryTransport::UpdateSessionBootstrap()
	{
		if (!m_SessionRetrySettings.enabled || m_SessionReady || m_SessionTimedOut || !m_HasOutstandingConnect)
			return;

		const bool timeoutReached = m_CurrentTick >= (m_LastConnectRequestTick + m_SessionRetrySettings.connectTimeoutTicks);
		if (!timeoutReached)
			return;

		if (m_ConnectRetryCount >= m_SessionRetrySettings.maxRetries)
		{
			m_SessionTimedOut = true;
			DisconnectSession();
			return;
		}

		const bool retryWindowReached = m_CurrentTick >= (m_LastRetryTick + m_SessionRetrySettings.retryIntervalTicks);
		if (!retryWindowReached)
			return;

		++m_ConnectRetryCount;
		SessionControlMessage message;
		message.type = SessionControlType::ConnectRequest;
		message.requestedClientID = m_LastRequestedClientID;
		message.serverTick = m_CurrentTick;
		m_ConnectRequestsToServer.push(message);
		m_LastConnectRequestTick = m_CurrentTick;
		m_LastRetryTick = m_CurrentTick;
	}

	bool InMemoryTransport::PollConnectRequestForServer(SessionControlMessage& outMessage)
	{
		if (m_ConnectRequestsToServer.empty())
			return false;

		outMessage = m_ConnectRequestsToServer.front();
		m_ConnectRequestsToServer.pop();
		return true;
	}

	void InMemoryTransport::AcceptConnectRequest(const SessionControlMessage& request)
	{
		SessionControlMessage response;
		response.type = SessionControlType::ConnectAccept;
		response.requestedClientID = request.requestedClientID;
		response.assignedClientID = request.requestedClientID != 0 ? request.requestedClientID : m_NextClientID++;
		if (request.requestedClientID >= m_NextClientID)
			m_NextClientID = request.requestedClientID + 1u;
		response.serverTick = m_CurrentTick;
		m_SessionControlToClient.push(response);
	}

	bool InMemoryTransport::PollSessionControlForClient(SessionControlMessage& outMessage)
	{
		if (m_SessionControlToClient.empty())
			return false;

		outMessage = m_SessionControlToClient.front();
		m_SessionControlToClient.pop();
		if (outMessage.type == SessionControlType::ConnectAccept)
		{
			m_AssignedClientID = outMessage.assignedClientID;
			m_SessionReady = (m_AssignedClientID != 0);
			m_HasOutstandingConnect = false;
		}
		else if (outMessage.type == SessionControlType::Disconnect)
		{
			m_AssignedClientID = 0;
			m_SessionReady = false;
			m_HasOutstandingConnect = false;
		}
		return true;
	}

	bool InMemoryTransport::IsSessionReady() const
	{
		return m_SessionReady;
	}

	bool InMemoryTransport::IsSessionTimedOut() const
	{
		return m_SessionTimedOut;
	}

	std::uint32_t InMemoryTransport::GetConnectRetryCount() const
	{
		return m_ConnectRetryCount;
	}

	ClientID InMemoryTransport::GetAssignedClientID() const
	{
		return m_AssignedClientID;
	}

	void InMemoryTransport::SendInputToServer(const InputMessage& message)
	{
		if (m_SimulationSettings.enabled)
		{
			SendInputToServerDelayed(message, m_CurrentTick);
			return;
		}
		m_InputToServer.push(message);
	}

	void InMemoryTransport::SendSnapshotToClient(const SnapshotMessage& message)
	{
		if (m_SimulationSettings.enabled)
		{
			SendSnapshotToClientDelayed(message, m_CurrentTick);
			return;
		}
		m_SnapshotToClient.push(message);
	}

	void InMemoryTransport::SendInputToServerDelayed(const InputMessage& message, NetTick deliverAtTick)
	{
		std::mt19937 rng(m_SimulationSettings.randomSeed ^ (deliverAtTick * 2654435761u) ^ message.command.tick);
		if (ShouldDropPacket<InputMessage>(m_SimulationSettings, rng))
			return;

		deliverAtTick += ComputeJitterTicks(m_SimulationSettings, rng);
		m_DelayedInputToServer.push_back({ deliverAtTick, message });
		MaybeReorderTail(m_DelayedInputToServer, m_SimulationSettings, rng);
	}

	void InMemoryTransport::SendSnapshotToClientDelayed(const SnapshotMessage& message, NetTick deliverAtTick)
	{
		std::mt19937 rng(m_SimulationSettings.randomSeed ^ (deliverAtTick * 2246822519u) ^ message.snapshot.tick);
		if (ShouldDropPacket<SnapshotMessage>(m_SimulationSettings, rng))
			return;

		deliverAtTick += ComputeJitterTicks(m_SimulationSettings, rng);
		m_DelayedSnapshotToClient.push_back({ deliverAtTick, message });
		MaybeReorderTail(m_DelayedSnapshotToClient, m_SimulationSettings, rng);
	}

	bool InMemoryTransport::PollInputForServer(InputMessage& outMessage)
	{
		if (m_InputToServer.empty())
			return false;

		outMessage = m_InputToServer.front();
		m_InputToServer.pop();
		return true;
	}

	bool InMemoryTransport::PollSnapshotForClient(SnapshotMessage& outMessage)
	{
		if (m_SnapshotToClient.empty())
			return false;

		outMessage = m_SnapshotToClient.front();
		m_SnapshotToClient.pop();
		return true;
	}

	void InMemoryTransport::AdvanceToTick(NetTick currentTick)
	{
		if (currentTick < m_CurrentTick)
			return;

		m_CurrentTick = currentTick;
		UpdateSessionBootstrap();

		while (!m_DelayedInputToServer.empty() && m_DelayedInputToServer.front().deliverAtTick <= m_CurrentTick)
		{
			m_InputToServer.push(m_DelayedInputToServer.front().payload);
			m_DelayedInputToServer.pop_front();
		}

		while (!m_DelayedSnapshotToClient.empty() && m_DelayedSnapshotToClient.front().deliverAtTick <= m_CurrentTick)
		{
			m_SnapshotToClient.push(m_DelayedSnapshotToClient.front().payload);
			m_DelayedSnapshotToClient.pop_front();
		}
	}

	void InMemoryTransport::Clear()
	{
		while (!m_ConnectRequestsToServer.empty())
			m_ConnectRequestsToServer.pop();
		while (!m_SessionControlToClient.empty())
			m_SessionControlToClient.pop();
		while (!m_InputToServer.empty())
			m_InputToServer.pop();
		while (!m_SnapshotToClient.empty())
			m_SnapshotToClient.pop();
		m_DelayedInputToServer.clear();
		m_DelayedSnapshotToClient.clear();
		m_CurrentTick = 0;
		m_NextClientID = 1u;
		m_AssignedClientID = 0u;
		m_SessionReady = false;
		m_SessionTimedOut = false;
		m_LastRequestedClientID = 0u;
		m_LastConnectRequestTick = 0u;
		m_LastRetryTick = 0u;
		m_ConnectRetryCount = 0u;
	}
}
