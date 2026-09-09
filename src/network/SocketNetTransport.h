#pragma once

#include <memory>

#include "network/NetTransport.h"

namespace MyEngine::Net
{
	class SocketNetTransport : public INetTransport
	{
	public:
		SocketNetTransport();
		~SocketNetTransport() override;

		bool Initialize(unsigned short serverPort = 28001, unsigned short clientPort = 28002);
		void Shutdown();
		bool IsInitialized() const;

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
		struct SocketNetTransportState;
		std::unique_ptr<SocketNetTransportState> m_State;
		std::unique_ptr<InMemoryTransport> m_Fallback;
		bool m_Initialized = false;
		unsigned short m_ServerPort = 0;
		unsigned short m_ClientPort = 0;
	};
}
