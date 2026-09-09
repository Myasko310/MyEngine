#pragma once

#include <deque>
#include <queue>

#include "network/NetTypes.h"

namespace MyEngine::Net
{
	struct NetworkSimulationSettings
	{
		bool enabled = false;
		float packetLossChance = 0.0f;
		NetTick jitterMinTicks = 0;
		NetTick jitterMaxTicks = 0;
		float reorderChance = 0.0f;
		std::uint32_t randomSeed = 1337u;
	};

	struct SessionRetrySettings
	{
		bool enabled = true;
		NetTick connectTimeoutTicks = 15u;
		NetTick retryIntervalTicks = 5u;
		std::uint32_t maxRetries = 3u;
	};

	class INetTransport
	{
	public:
		virtual ~INetTransport() = default;

		virtual void SetSimulationSettings(const NetworkSimulationSettings& settings) = 0;
		virtual const NetworkSimulationSettings& GetSimulationSettings() const = 0;

		virtual void BeginServerSession() = 0;
		virtual void SetSessionRetrySettings(const SessionRetrySettings& settings) = 0;
		virtual const SessionRetrySettings& GetSessionRetrySettings() const = 0;
		virtual void SendConnectRequest(ClientID requestedClientID = 0) = 0;
		virtual void DisconnectSession() = 0;
		virtual void UpdateSessionBootstrap() = 0;
		virtual bool PollConnectRequestForServer(SessionControlMessage& outMessage) = 0;
		virtual void AcceptConnectRequest(const SessionControlMessage& request) = 0;
		virtual bool PollSessionControlForClient(SessionControlMessage& outMessage) = 0;
		virtual bool IsSessionReady() const = 0;
		virtual bool IsSessionTimedOut() const = 0;
		virtual std::uint32_t GetConnectRetryCount() const = 0;
		virtual ClientID GetAssignedClientID() const = 0;

		virtual void SendInputToServer(const InputMessage& message) = 0;
		virtual void SendSnapshotToClient(const SnapshotMessage& message) = 0;
		virtual void SendInputToServerDelayed(const InputMessage& message, NetTick deliverAtTick) = 0;
		virtual void SendSnapshotToClientDelayed(const SnapshotMessage& message, NetTick deliverAtTick) = 0;

		virtual bool PollInputForServer(InputMessage& outMessage) = 0;
		virtual bool PollSnapshotForClient(SnapshotMessage& outMessage) = 0;
		virtual void AdvanceToTick(NetTick currentTick) = 0;

		virtual void Clear() = 0;
	};

	class InMemoryTransport : public INetTransport
	{
	public:
		void SetSimulationSettings(const NetworkSimulationSettings& settings) override;
		const NetworkSimulationSettings& GetSimulationSettings() const override;

		void BeginServerSession() override;
		void SetSessionRetrySettings(const SessionRetrySettings& settings) override;
		const SessionRetrySettings& GetSessionRetrySettings() const override;
		void SendConnectRequest(ClientID requestedClientID = 0) override;
		void DisconnectSession() override;
		void UpdateSessionBootstrap() override;
		bool PollConnectRequestForServer(SessionControlMessage& outMessage) override;
		void AcceptConnectRequest(const SessionControlMessage& request) override;
		bool PollSessionControlForClient(SessionControlMessage& outMessage) override;
		bool IsSessionReady() const override;
		bool IsSessionTimedOut() const override;
		std::uint32_t GetConnectRetryCount() const override;
		ClientID GetAssignedClientID() const override;

		void SendInputToServer(const InputMessage& message) override;
		void SendSnapshotToClient(const SnapshotMessage& message) override;
		void SendInputToServerDelayed(const InputMessage& message, NetTick deliverAtTick) override;
		void SendSnapshotToClientDelayed(const SnapshotMessage& message, NetTick deliverAtTick) override;

		bool PollInputForServer(InputMessage& outMessage) override;
		bool PollSnapshotForClient(SnapshotMessage& outMessage) override;
		void AdvanceToTick(NetTick currentTick) override;

		void Clear() override;

	private:
		template <typename T>
		struct DelayedMessage
		{
			NetTick deliverAtTick = 0;
			T payload{};
		};

		std::queue<SessionControlMessage> m_ConnectRequestsToServer;
		std::queue<SessionControlMessage> m_SessionControlToClient;
		std::queue<InputMessage> m_InputToServer;
		std::queue<SnapshotMessage> m_SnapshotToClient;
		std::deque<DelayedMessage<InputMessage>> m_DelayedInputToServer;
		std::deque<DelayedMessage<SnapshotMessage>> m_DelayedSnapshotToClient;
		NetTick m_CurrentTick = 0;
		NetworkSimulationSettings m_SimulationSettings{};
		SessionRetrySettings m_SessionRetrySettings{};
		ClientID m_NextClientID = 1u;
		ClientID m_AssignedClientID = 0u;
		bool m_SessionReady = false;
		bool m_SessionTimedOut = false;
		bool m_HasOutstandingConnect = false;
		ClientID m_LastRequestedClientID = 0u;
		NetTick m_LastConnectRequestTick = 0u;
		NetTick m_LastRetryTick = 0u;
		std::uint32_t m_ConnectRetryCount = 0u;
	};
}
