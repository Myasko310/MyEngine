#include "network/SocketNetTransport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

namespace MyEngine::Net
{
	namespace
	{
		enum class PacketType : std::uint8_t
		{
			SessionControl = 1,
			Input = 2,
			Snapshot = 3,
			Heartbeat = 4
		};

		enum PacketFlags : std::uint8_t
		{
			PacketFlagNone = 0,
			PacketFlagReliable = 1 << 0,
			PacketFlagFragmented = 1 << 1
		};

		constexpr std::uint16_t kPacketMagic = 0x4D45;
		constexpr std::size_t kMaxPacketBytes = 60 * 1024;
		constexpr NetTick kReliableResendIntervalTicks = 3u;
		constexpr NetTick kHeartbeatIntervalTicks = 10u;
		constexpr NetTick kInactivityTimeoutTicks = 45u;
		constexpr std::size_t kPacketHeaderSize = sizeof(std::uint16_t) + sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint32_t) + sizeof(std::uint32_t) + sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::uint16_t);
		constexpr std::size_t kMaxFragmentPayloadBytes = kMaxPacketBytes - kPacketHeaderSize;

		struct PacketHeader
		{
			std::uint16_t magic = kPacketMagic;
			std::uint8_t type = 0;
			std::uint8_t flags = PacketFlagNone;
			std::uint32_t sequence = 0;
			std::uint32_t ack = 0;
			std::uint32_t fragmentGroup = 0;
			std::uint16_t fragmentIndex = 0;
			std::uint16_t fragmentCount = 1;
		};

		struct ByteWriter
		{
			std::vector<std::uint8_t> bytes;

			template <typename T>
			void Write(const T& value)
			{
				const auto* data = reinterpret_cast<const std::uint8_t*>(&value);
				bytes.insert(bytes.end(), data, data + sizeof(T));
			}

			void WriteBytes(const std::vector<std::uint8_t>& payload)
			{
				bytes.insert(bytes.end(), payload.begin(), payload.end());
			}

			void WriteString(const std::string& value)
			{
				const std::uint32_t size = static_cast<std::uint32_t>(value.size());
				Write(size);
				bytes.insert(bytes.end(), value.begin(), value.end());
			}
		};

		struct ByteReader
		{
			const std::uint8_t* data = nullptr;
			std::size_t size = 0;
			std::size_t offset = 0;

			template <typename T>
			bool Read(T& outValue)
			{
				if (offset + sizeof(T) > size)
					return false;
				std::memcpy(&outValue, data + offset, sizeof(T));
				offset += sizeof(T);
				return true;
			}

			bool ReadString(std::string& outValue)
			{
				std::uint32_t len = 0;
				if (!Read(len) || offset + len > size)
					return false;
				outValue.assign(reinterpret_cast<const char*>(data + offset), len);
				offset += len;
				return true;
			}
		};

		void SerializeHeader(ByteWriter& writer, const PacketHeader& header)
		{
			writer.Write(header.magic);
			writer.Write(header.type);
			writer.Write(header.flags);
			writer.Write(header.sequence);
			writer.Write(header.ack);
			writer.Write(header.fragmentGroup);
			writer.Write(header.fragmentIndex);
			writer.Write(header.fragmentCount);
		}

		bool DeserializeHeader(ByteReader& reader, PacketHeader& header)
		{
			return reader.Read(header.magic)
				&& reader.Read(header.type)
				&& reader.Read(header.flags)
				&& reader.Read(header.sequence)
				&& reader.Read(header.ack)
				&& reader.Read(header.fragmentGroup)
				&& reader.Read(header.fragmentIndex)
				&& reader.Read(header.fragmentCount);
		}

		void SerializeSessionControl(ByteWriter& writer, const SessionControlMessage& message)
		{
			writer.Write(static_cast<std::uint8_t>(message.type));
			writer.Write(message.requestedClientID);
			writer.Write(message.assignedClientID);
			writer.Write(message.serverTick);
		}

		bool DeserializeSessionControl(ByteReader& reader, SessionControlMessage& outMessage)
		{
			std::uint8_t type = 0;
			if (!reader.Read(type))
				return false;
			outMessage.type = static_cast<SessionControlType>(type);
			return reader.Read(outMessage.requestedClientID)
				&& reader.Read(outMessage.assignedClientID)
				&& reader.Read(outMessage.serverTick);
		}

		void SerializeInput(ByteWriter& writer, const InputMessage& message)
		{
			writer.Write(message.clientID);
			writer.Write(message.command.tick);
			writer.Write(message.command.moveAxis.x);
			writer.Write(message.command.moveAxis.y);
			writer.Write(message.command.jumpPressed);
		}

		bool DeserializeInput(ByteReader& reader, InputMessage& outMessage)
		{
			if (!reader.Read(outMessage.clientID))
				return false;
			if (!reader.Read(outMessage.command.tick))
				return false;
			if (!reader.Read(outMessage.command.moveAxis.x))
				return false;
			if (!reader.Read(outMessage.command.moveAxis.y))
				return false;
			return reader.Read(outMessage.command.jumpPressed);
		}

		void SerializeSnapshot(ByteWriter& writer, const SnapshotMessage& message)
		{
			writer.Write(message.clientID);
			writer.Write(message.snapshot.tick);
			const std::uint32_t entityCount = static_cast<std::uint32_t>(message.snapshot.entities.size());
			writer.Write(entityCount);
			for (const auto& entity : message.snapshot.entities)
			{
				writer.Write(entity.entityID);
				writer.Write(entity.position.x);
				writer.Write(entity.position.y);
				writer.Write(entity.position.z);
				writer.Write(entity.rotation.x);
				writer.Write(entity.rotation.y);
				writer.Write(entity.rotation.z);
				writer.Write(entity.velocity.x);
				writer.Write(entity.velocity.y);
				writer.Write(entity.velocity.z);
				writer.Write(entity.isGrounded);
				writer.Write(entity.animationPlaying);
				writer.Write(entity.audioPlaying);
				writer.Write(entity.activeAnimationClipIndex);
				writer.Write(entity.animationTimeSeconds);
				writer.WriteString(entity.audioEventName);
			}

			const std::uint32_t terrainPatchCount = static_cast<std::uint32_t>(message.snapshot.terrainPatches.size());
			writer.Write(terrainPatchCount);
			for (const auto& terrainPatch : message.snapshot.terrainPatches)
			{
				writer.Write(terrainPatch.terrainEntityID);
				writer.Write(terrainPatch.resolution);
				writer.Write(terrainPatch.minRow);
				writer.Write(terrainPatch.maxRow);
				writer.Write(terrainPatch.minCol);
				writer.Write(terrainPatch.maxCol);
				writer.Write(terrainPatch.authoredTick);
				const std::uint32_t pointCount = static_cast<std::uint32_t>(terrainPatch.points.size());
				writer.Write(pointCount);
				for (const auto& point : terrainPatch.points)
				{
					writer.Write(point.row);
					writer.Write(point.col);
					writer.Write(point.heightBefore);
					writer.Write(point.heightAfter);
				}
			}
		}

		bool DeserializeSnapshot(ByteReader& reader, SnapshotMessage& outMessage)
		{
			if (!reader.Read(outMessage.clientID))
				return false;
			if (!reader.Read(outMessage.snapshot.tick))
				return false;
			std::uint32_t entityCount = 0;
			if (!reader.Read(entityCount))
				return false;
			outMessage.snapshot.entities.clear();
			outMessage.snapshot.entities.reserve(entityCount);
			for (std::uint32_t i = 0; i < entityCount; ++i)
			{
				ReplicatedEntityState entity;
				if (!reader.Read(entity.entityID)
					|| !reader.Read(entity.position.x)
					|| !reader.Read(entity.position.y)
					|| !reader.Read(entity.position.z)
					|| !reader.Read(entity.rotation.x)
					|| !reader.Read(entity.rotation.y)
					|| !reader.Read(entity.rotation.z)
					|| !reader.Read(entity.velocity.x)
					|| !reader.Read(entity.velocity.y)
					|| !reader.Read(entity.velocity.z)
					|| !reader.Read(entity.isGrounded)
					|| !reader.Read(entity.animationPlaying)
					|| !reader.Read(entity.audioPlaying)
					|| !reader.Read(entity.activeAnimationClipIndex)
					|| !reader.Read(entity.animationTimeSeconds)
					|| !reader.ReadString(entity.audioEventName))
				{
					return false;
				}
				outMessage.snapshot.entities.push_back(std::move(entity));
			}

			std::uint32_t terrainPatchCount = 0;
			if (!reader.Read(terrainPatchCount))
				return false;
			outMessage.snapshot.terrainPatches.clear();
			outMessage.snapshot.terrainPatches.reserve(terrainPatchCount);
			for (std::uint32_t i = 0; i < terrainPatchCount; ++i)
			{
				TerrainPatchDelta terrainPatch;
				if (!reader.Read(terrainPatch.terrainEntityID)
					|| !reader.Read(terrainPatch.resolution)
					|| !reader.Read(terrainPatch.minRow)
					|| !reader.Read(terrainPatch.maxRow)
					|| !reader.Read(terrainPatch.minCol)
					|| !reader.Read(terrainPatch.maxCol)
					|| !reader.Read(terrainPatch.authoredTick))
				{
					return false;
				}

				std::uint32_t pointCount = 0;
				if (!reader.Read(pointCount))
					return false;
				terrainPatch.points.reserve(pointCount);
				for (std::uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
				{
					TerrainPatchPointDelta point;
					if (!reader.Read(point.row)
						|| !reader.Read(point.col)
						|| !reader.Read(point.heightBefore)
						|| !reader.Read(point.heightAfter))
					{
						return false;
					}
					terrainPatch.points.push_back(point);
				}

				outMessage.snapshot.terrainPatches.push_back(std::move(terrainPatch));
			}
			return true;
		}

		std::vector<std::uint8_t> SlicePayload(const std::vector<std::uint8_t>& payload, std::size_t offset, std::size_t length)
		{
			const auto begin = payload.begin() + static_cast<std::ptrdiff_t>(offset);
			const auto end = begin + static_cast<std::ptrdiff_t>(length);
			return std::vector<std::uint8_t>(begin, end);
		}
	}

	struct SocketNetTransport::SocketNetTransportState
	{
#ifdef _WIN32
		SOCKET serverSocket = INVALID_SOCKET;
		SOCKET clientSocket = INVALID_SOCKET;
		sockaddr_in serverAddress{};
		sockaddr_in clientAddress{};
#endif
		template <typename T>
		struct DelayedMessage
		{
			NetTick deliverAtTick = 0;
			T payload{};
		};

		struct ReliablePacket
		{
			bool fromServer = false;
			std::uint32_t sequence = 0;
			NetTick lastSentTick = 0;
			std::vector<std::uint8_t> bytes;
		};

		struct FragmentAccumulator
		{
			PacketType type = PacketType::Snapshot;
			std::uint8_t flags = PacketFlagNone;
			std::uint16_t fragmentCount = 0;
			std::vector<std::vector<std::uint8_t>> parts;
			std::size_t receivedParts = 0;
		};

		std::queue<SessionControlMessage> connectRequestsToServer;
		std::queue<SessionControlMessage> sessionControlToClient;
		std::queue<InputMessage> inputToServer;
		std::queue<SnapshotMessage> snapshotToClient;
		std::deque<DelayedMessage<InputMessage>> delayedInputToServer;
		std::deque<DelayedMessage<SnapshotMessage>> delayedSnapshotToClient;
		std::vector<ReliablePacket> reliablePackets;
		std::unordered_map<std::uint32_t, FragmentAccumulator> reassemblyFromClient;
		std::unordered_map<std::uint32_t, FragmentAccumulator> reassemblyFromServer;
		NetTick currentTick = 0;
		SessionRetrySettings retrySettings{};
		ClientID nextClientID = 1u;
		ClientID assignedClientID = 0u;
		bool sessionReady = false;
		bool sessionTimedOut = false;
		bool hasOutstandingConnect = false;
		ClientID lastRequestedClientID = 0u;
		NetTick lastConnectRequestTick = 0u;
		NetTick lastRetryTick = 0u;
		std::uint32_t connectRetryCount = 0u;
		std::uint32_t nextSequenceClientToServer = 1u;
		std::uint32_t nextSequenceServerToClient = 1u;
		std::uint32_t lastReceivedSequenceFromClient = 0u;
		std::uint32_t lastReceivedSequenceFromServer = 0u;
		std::uint32_t nextFragmentGroup = 1u;
		NetTick lastTrafficFromClientTick = 0u;
		NetTick lastTrafficFromServerTick = 0u;
		NetTick lastHeartbeatFromClientTick = 0u;
		NetTick lastHeartbeatFromServerTick = 0u;
	};

	SocketNetTransport::SocketNetTransport()
		: m_State(std::make_unique<SocketNetTransportState>())
		, m_Fallback(std::make_unique<InMemoryTransport>())
	{
	}

	SocketNetTransport::~SocketNetTransport()
	{
		Shutdown();
	}

	bool SocketNetTransport::Initialize(unsigned short serverPort, unsigned short clientPort)
	{
		m_ServerPort = serverPort;
		m_ClientPort = clientPort;
#ifdef _WIN32
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;

		m_State->serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		m_State->clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_State->serverSocket == INVALID_SOCKET || m_State->clientSocket == INVALID_SOCKET)
		{
			Shutdown();
			return false;
		}

		u_long nonBlocking = 1;
		ioctlsocket(m_State->serverSocket, FIONBIO, &nonBlocking);
		ioctlsocket(m_State->clientSocket, FIONBIO, &nonBlocking);

		m_State->serverAddress = {};
		m_State->serverAddress.sin_family = AF_INET;
		m_State->serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		m_State->serverAddress.sin_port = htons(m_ServerPort);
		if (bind(m_State->serverSocket, reinterpret_cast<sockaddr*>(&m_State->serverAddress), sizeof(m_State->serverAddress)) == SOCKET_ERROR)
		{
			Shutdown();
			return false;
		}

		m_State->clientAddress = {};
		m_State->clientAddress.sin_family = AF_INET;
		m_State->clientAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		m_State->clientAddress.sin_port = htons(m_ClientPort);
		if (bind(m_State->clientSocket, reinterpret_cast<sockaddr*>(&m_State->clientAddress), sizeof(m_State->clientAddress)) == SOCKET_ERROR)
		{
			Shutdown();
			return false;
		}
#endif
		m_Initialized = true;
		return true;
	}

	void SocketNetTransport::Shutdown()
	{
#ifdef _WIN32
		if (m_State && m_State->serverSocket != INVALID_SOCKET)
		{
			closesocket(m_State->serverSocket);
			m_State->serverSocket = INVALID_SOCKET;
		}
		if (m_State && m_State->clientSocket != INVALID_SOCKET)
		{
			closesocket(m_State->clientSocket);
			m_State->clientSocket = INVALID_SOCKET;
		}
		WSACleanup();
#endif
		m_Initialized = false;
		Clear();
	}

	bool SocketNetTransport::IsInitialized() const
	{
		return m_Initialized;
	}

	void SocketNetTransport::SetSimulationSettings(const NetworkSimulationSettings& settings)
	{
		m_Fallback->SetSimulationSettings(settings);
	}

	const NetworkSimulationSettings& SocketNetTransport::GetSimulationSettings() const
	{
		return m_Fallback->GetSimulationSettings();
	}

	void SocketNetTransport::BeginServerSession()
	{
		m_State->nextClientID = 1u;
		m_State->sessionTimedOut = false;
		m_State->sessionReady = false;
		m_State->assignedClientID = 0u;
	}

	void SocketNetTransport::SetSessionRetrySettings(const SessionRetrySettings& settings)
	{
		m_State->retrySettings = settings;
	}

	const SessionRetrySettings& SocketNetTransport::GetSessionRetrySettings() const
	{
		return m_State->retrySettings;
	}

	void SocketNetTransport::SendConnectRequest(ClientID requestedClientID)
	{
		SessionControlMessage message;
		message.type = SessionControlType::ConnectRequest;
		message.requestedClientID = requestedClientID;
		message.serverTick = m_State->currentTick;
		m_State->lastRequestedClientID = requestedClientID;
		m_State->lastConnectRequestTick = m_State->currentTick;
		m_State->lastRetryTick = m_State->currentTick;
		m_State->hasOutstandingConnect = true;
		m_State->sessionTimedOut = false;
		m_State->connectRetryCount = 0u;
		m_State->assignedClientID = 0u;
		m_State->sessionReady = false;

#ifdef _WIN32
		auto buildPayload = [&](const SessionControlMessage& control)
		{
			ByteWriter payloadWriter;
			SerializeSessionControl(payloadWriter, control);
			return payloadWriter.bytes;
		};
		auto sendPacket = [&](PacketType type, bool fromServer, bool reliable, const std::vector<std::uint8_t>& payload, std::uint32_t fragmentGroup = 0u, std::uint16_t fragmentIndex = 0u, std::uint16_t fragmentCount = 1u)
		{
			PacketHeader header;
			header.type = static_cast<std::uint8_t>(type);
			header.flags = reliable ? PacketFlagReliable : PacketFlagNone;
			if (fragmentCount > 1u)
				header.flags = static_cast<std::uint8_t>(header.flags | PacketFlagFragmented);
			header.sequence = fromServer ? m_State->nextSequenceServerToClient++ : m_State->nextSequenceClientToServer++;
			header.ack = fromServer ? m_State->lastReceivedSequenceFromClient : m_State->lastReceivedSequenceFromServer;
			header.fragmentGroup = fragmentGroup;
			header.fragmentIndex = fragmentIndex;
			header.fragmentCount = fragmentCount;

			ByteWriter datagram;
			SerializeHeader(datagram, header);
			datagram.WriteBytes(payload);
			const SOCKET sendSocket = fromServer ? m_State->serverSocket : m_State->clientSocket;
			const sockaddr_in& toAddr = fromServer ? m_State->clientAddress : m_State->serverAddress;
			sendto(sendSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&toAddr), sizeof(toAddr));
			if (reliable)
				m_State->reliablePackets.push_back({ fromServer, header.sequence, m_State->currentTick, std::move(datagram.bytes) });
		};

		sendPacket(PacketType::SessionControl, false, true, buildPayload(message));
#else
		m_State->connectRequestsToServer.push(message);
#endif
	}

	void SocketNetTransport::DisconnectSession()
	{
		SessionControlMessage message;
		message.type = SessionControlType::Disconnect;
		message.assignedClientID = m_State->assignedClientID;
		message.serverTick = m_State->currentTick;
#ifdef _WIN32
		ByteWriter payloadWriter;
		SerializeSessionControl(payloadWriter, message);

		PacketHeader header;
		header.type = static_cast<std::uint8_t>(PacketType::SessionControl);
		header.flags = PacketFlagReliable;
		header.sequence = m_State->nextSequenceClientToServer++;
		header.ack = m_State->lastReceivedSequenceFromServer;

		ByteWriter datagram;
		SerializeHeader(datagram, header);
		datagram.WriteBytes(payloadWriter.bytes);
		sendto(m_State->clientSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
			reinterpret_cast<const sockaddr*>(&m_State->serverAddress), sizeof(m_State->serverAddress));
		m_State->reliablePackets.push_back({ false, header.sequence, m_State->currentTick, std::move(datagram.bytes) });
#else
		m_State->connectRequestsToServer.push(message);
#endif
		m_State->sessionReady = false;
		m_State->hasOutstandingConnect = false;
	}

	void SocketNetTransport::UpdateSessionBootstrap()
	{
		if (!m_State->hasOutstandingConnect || !m_State->retrySettings.enabled)
			return;
		if (m_State->sessionReady || m_State->sessionTimedOut)
			return;

		const auto elapsedSinceRequest = m_State->currentTick - m_State->lastConnectRequestTick;
		if (elapsedSinceRequest >= m_State->retrySettings.connectTimeoutTicks)
		{
			m_State->sessionTimedOut = true;
			m_State->hasOutstandingConnect = false;
			SessionControlMessage disconnectMessage;
			disconnectMessage.type = SessionControlType::Disconnect;
			disconnectMessage.serverTick = m_State->currentTick;
			m_State->sessionControlToClient.push(disconnectMessage);
			return;
		}

		const auto elapsedSinceRetry = m_State->currentTick - m_State->lastRetryTick;
		if (elapsedSinceRetry < m_State->retrySettings.retryIntervalTicks)
			return;

		if (m_State->connectRetryCount >= m_State->retrySettings.maxRetries)
		{
			m_State->sessionTimedOut = true;
			m_State->hasOutstandingConnect = false;
			SessionControlMessage disconnectMessage;
			disconnectMessage.type = SessionControlType::Disconnect;
			disconnectMessage.serverTick = m_State->currentTick;
			m_State->sessionControlToClient.push(disconnectMessage);
			return;
		}

		++m_State->connectRetryCount;
		m_State->lastRetryTick = m_State->currentTick;

		SessionControlMessage retryMessage;
		retryMessage.type = SessionControlType::ConnectRequest;
		retryMessage.requestedClientID = m_State->lastRequestedClientID;
		retryMessage.serverTick = m_State->currentTick;
#ifdef _WIN32
		ByteWriter payloadWriter;
		SerializeSessionControl(payloadWriter, retryMessage);
		PacketHeader header;
		header.type = static_cast<std::uint8_t>(PacketType::SessionControl);
		header.flags = PacketFlagReliable;
		header.sequence = m_State->nextSequenceClientToServer++;
		header.ack = m_State->lastReceivedSequenceFromServer;
		ByteWriter datagram;
		SerializeHeader(datagram, header);
		datagram.WriteBytes(payloadWriter.bytes);
		sendto(m_State->clientSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
			reinterpret_cast<const sockaddr*>(&m_State->serverAddress), sizeof(m_State->serverAddress));
		m_State->reliablePackets.push_back({ false, header.sequence, m_State->currentTick, std::move(datagram.bytes) });
#else
		m_State->connectRequestsToServer.push(retryMessage);
#endif
	}

	bool SocketNetTransport::PollConnectRequestForServer(SessionControlMessage& outMessage)
	{
		if (m_State->connectRequestsToServer.empty())
			return false;
		outMessage = m_State->connectRequestsToServer.front();
		m_State->connectRequestsToServer.pop();
		return true;
	}

	void SocketNetTransport::AcceptConnectRequest(const SessionControlMessage& request)
	{
		SessionControlMessage accepted;
		accepted.type = SessionControlType::ConnectAccept;
		accepted.requestedClientID = request.requestedClientID;
		accepted.assignedClientID = (request.requestedClientID != 0u) ? request.requestedClientID : m_State->nextClientID++;
		accepted.serverTick = m_State->currentTick;
#ifdef _WIN32
		ByteWriter payloadWriter;
		SerializeSessionControl(payloadWriter, accepted);
		PacketHeader header;
		header.type = static_cast<std::uint8_t>(PacketType::SessionControl);
		header.flags = PacketFlagReliable;
		header.sequence = m_State->nextSequenceServerToClient++;
		header.ack = m_State->lastReceivedSequenceFromClient;
		ByteWriter datagram;
		SerializeHeader(datagram, header);
		datagram.WriteBytes(payloadWriter.bytes);
		sendto(m_State->serverSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
			reinterpret_cast<const sockaddr*>(&m_State->clientAddress), sizeof(m_State->clientAddress));
		m_State->reliablePackets.push_back({ true, header.sequence, m_State->currentTick, std::move(datagram.bytes) });
#else
		m_State->sessionControlToClient.push(accepted);
#endif
	}

	bool SocketNetTransport::PollSessionControlForClient(SessionControlMessage& outMessage)
	{
		if (m_State->sessionControlToClient.empty())
			return false;
		outMessage = m_State->sessionControlToClient.front();
		m_State->sessionControlToClient.pop();
		if (outMessage.type == SessionControlType::ConnectAccept)
		{
			m_State->assignedClientID = outMessage.assignedClientID;
			m_State->sessionReady = true;
			m_State->hasOutstandingConnect = false;
			m_State->sessionTimedOut = false;
			m_State->lastTrafficFromClientTick = m_State->currentTick;
			m_State->lastTrafficFromServerTick = m_State->currentTick;
			m_State->lastHeartbeatFromClientTick = m_State->currentTick;
			m_State->lastHeartbeatFromServerTick = m_State->currentTick;
		}
		if (outMessage.type == SessionControlType::Disconnect)
		{
			m_State->sessionReady = false;
			m_State->assignedClientID = 0u;
		}
		return true;
	}

	bool SocketNetTransport::IsSessionReady() const
	{
		return m_State->sessionReady;
	}

	bool SocketNetTransport::IsSessionTimedOut() const
	{
		return m_State->sessionTimedOut;
	}

	std::uint32_t SocketNetTransport::GetConnectRetryCount() const
	{
		return m_State->connectRetryCount;
	}

	ClientID SocketNetTransport::GetAssignedClientID() const
	{
		return m_State->assignedClientID;
	}

	void SocketNetTransport::SendInputToServer(const InputMessage& message)
	{
#ifdef _WIN32
		ByteWriter payloadWriter;
		SerializeInput(payloadWriter, message);
		PacketHeader header;
		header.type = static_cast<std::uint8_t>(PacketType::Input);
		header.sequence = m_State->nextSequenceClientToServer++;
		header.ack = m_State->lastReceivedSequenceFromServer;
		ByteWriter datagram;
		SerializeHeader(datagram, header);
		datagram.WriteBytes(payloadWriter.bytes);
		sendto(m_State->clientSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
			reinterpret_cast<const sockaddr*>(&m_State->serverAddress), sizeof(m_State->serverAddress));
#else
		m_State->inputToServer.push(message);
#endif
	}

	void SocketNetTransport::SendSnapshotToClient(const SnapshotMessage& message)
	{
#ifdef _WIN32
		ByteWriter payloadWriter;
		SerializeSnapshot(payloadWriter, message);
		if (payloadWriter.bytes.empty())
			return;

		auto sendFragment = [&](const std::vector<std::uint8_t>& fragmentPayload, std::uint16_t fragmentIndex, std::uint16_t fragmentCount, std::uint32_t fragmentGroup)
		{
			PacketHeader header;
			header.type = static_cast<std::uint8_t>(PacketType::Snapshot);
			header.flags = PacketFlagReliable;
			if (fragmentCount > 1u)
				header.flags = static_cast<std::uint8_t>(header.flags | PacketFlagFragmented);
			header.sequence = m_State->nextSequenceServerToClient++;
			header.ack = m_State->lastReceivedSequenceFromClient;
			header.fragmentGroup = fragmentGroup;
			header.fragmentIndex = fragmentIndex;
			header.fragmentCount = fragmentCount;
			ByteWriter datagram;
			SerializeHeader(datagram, header);
			datagram.WriteBytes(fragmentPayload);
			sendto(m_State->serverSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&m_State->clientAddress), sizeof(m_State->clientAddress));
			m_State->reliablePackets.push_back({ true, header.sequence, m_State->currentTick, std::move(datagram.bytes) });
		};

		if (payloadWriter.bytes.size() + kPacketHeaderSize <= kMaxPacketBytes)
		{
			sendFragment(payloadWriter.bytes, 0u, 1u, 0u);
			return;
		}

		const std::uint32_t fragmentGroup = m_State->nextFragmentGroup++;
		const std::size_t fragmentCountSize = (payloadWriter.bytes.size() + kMaxFragmentPayloadBytes - 1u) / kMaxFragmentPayloadBytes;
		const auto fragmentCount = static_cast<std::uint16_t>(std::min<std::size_t>(fragmentCountSize, static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())));
		for (std::uint16_t fragmentIndex = 0; fragmentIndex < fragmentCount; ++fragmentIndex)
		{
			const std::size_t offset = static_cast<std::size_t>(fragmentIndex) * kMaxFragmentPayloadBytes;
			const std::size_t remaining = payloadWriter.bytes.size() - offset;
			const std::size_t chunk = std::min(remaining, kMaxFragmentPayloadBytes);
			sendFragment(SlicePayload(payloadWriter.bytes, offset, chunk), fragmentIndex, fragmentCount, fragmentGroup);
		}
#else
		m_State->snapshotToClient.push(message);
#endif
	}

	void SocketNetTransport::SendInputToServerDelayed(const InputMessage& message, NetTick deliverAtTick)
	{
		m_State->delayedInputToServer.push_back({ deliverAtTick, message });
	}

	void SocketNetTransport::SendSnapshotToClientDelayed(const SnapshotMessage& message, NetTick deliverAtTick)
	{
		m_State->delayedSnapshotToClient.push_back({ deliverAtTick, message });
	}

	bool SocketNetTransport::PollInputForServer(InputMessage& outMessage)
	{
		if (m_State->inputToServer.empty())
			return false;
		outMessage = m_State->inputToServer.front();
		m_State->inputToServer.pop();
		return true;
	}

	bool SocketNetTransport::PollSnapshotForClient(SnapshotMessage& outMessage)
	{
		if (m_State->snapshotToClient.empty())
			return false;
		outMessage = m_State->snapshotToClient.front();
		m_State->snapshotToClient.pop();
		return true;
	}

	void SocketNetTransport::AdvanceToTick(NetTick currentTick)
	{
		m_State->currentTick = currentTick;

		auto processAck = [&](bool ackFromClient, std::uint32_t ackValue)
		{
			if (ackValue == 0u)
				return;
			m_State->reliablePackets.erase(
				std::remove_if(m_State->reliablePackets.begin(), m_State->reliablePackets.end(), [&](const SocketNetTransportState::ReliablePacket& packet)
				{
					const bool packetMatchesDirection = ackFromClient ? packet.fromServer : !packet.fromServer;
					return packetMatchesDirection && packet.sequence <= ackValue;
				}),
				m_State->reliablePackets.end());
		};

		auto enqueueDecodedPayload = [&](PacketType type, const std::vector<std::uint8_t>& payload, bool fromClient)
		{
			ByteReader payloadReader{ payload.data(), payload.size(), 0 };
			switch (type)
			{
			case PacketType::SessionControl:
			{
				SessionControlMessage message;
				if (!DeserializeSessionControl(payloadReader, message))
					return;
				if (fromClient)
					m_State->connectRequestsToServer.push(message);
				else
					m_State->sessionControlToClient.push(message);
				break;
			}
			case PacketType::Input:
			{
				InputMessage message;
				if (DeserializeInput(payloadReader, message))
					m_State->inputToServer.push(message);
				break;
			}
			case PacketType::Snapshot:
			{
				SnapshotMessage message;
				if (DeserializeSnapshot(payloadReader, message))
					m_State->snapshotToClient.push(std::move(message));
				break;
			}
			default:
				break;
			}
		};

		auto enqueueFromDatagram = [&](const PacketHeader& header, const std::vector<std::uint8_t>& payload, bool fromClient)
		{
			if ((header.flags & PacketFlagFragmented) == 0)
			{
				enqueueDecodedPayload(static_cast<PacketType>(header.type), payload, fromClient);
				return;
			}

			auto& reassemblyMap = fromClient ? m_State->reassemblyFromClient : m_State->reassemblyFromServer;
			auto& accumulator = reassemblyMap[header.fragmentGroup];
			if (accumulator.parts.empty())
			{
				accumulator.type = static_cast<PacketType>(header.type);
				accumulator.flags = header.flags;
				accumulator.fragmentCount = header.fragmentCount;
				accumulator.parts.resize(header.fragmentCount);
			}
			if (header.fragmentIndex >= accumulator.parts.size())
				return;
			if (accumulator.parts[header.fragmentIndex].empty())
				++accumulator.receivedParts;
			accumulator.parts[header.fragmentIndex] = payload;
			if (accumulator.receivedParts < accumulator.fragmentCount)
				return;

			std::vector<std::uint8_t> merged;
			for (const auto& part : accumulator.parts)
				merged.insert(merged.end(), part.begin(), part.end());
			enqueueDecodedPayload(accumulator.type, merged, fromClient);
			reassemblyMap.erase(header.fragmentGroup);
		};

		while (!m_State->delayedInputToServer.empty() && m_State->delayedInputToServer.front().deliverAtTick <= currentTick)
		{
			SendInputToServer(m_State->delayedInputToServer.front().payload);
			m_State->delayedInputToServer.pop_front();
		}

		while (!m_State->delayedSnapshotToClient.empty() && m_State->delayedSnapshotToClient.front().deliverAtTick <= currentTick)
		{
			SendSnapshotToClient(m_State->delayedSnapshotToClient.front().payload);
			m_State->delayedSnapshotToClient.pop_front();
		}

#ifdef _WIN32
		for (auto& pending : m_State->reliablePackets)
		{
			if ((currentTick - pending.lastSentTick) < kReliableResendIntervalTicks)
				continue;
			const SOCKET sendSocket = pending.fromServer ? m_State->serverSocket : m_State->clientSocket;
			const sockaddr_in& toAddr = pending.fromServer ? m_State->clientAddress : m_State->serverAddress;
			sendto(sendSocket, reinterpret_cast<const char*>(pending.bytes.data()), static_cast<int>(pending.bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&toAddr), sizeof(toAddr));
			pending.lastSentTick = currentTick;
		}

		auto sendHeartbeat = [&](bool fromServer)
		{
			PacketHeader header;
			header.type = static_cast<std::uint8_t>(PacketType::Heartbeat);
			header.sequence = fromServer ? m_State->nextSequenceServerToClient++ : m_State->nextSequenceClientToServer++;
			header.ack = fromServer ? m_State->lastReceivedSequenceFromClient : m_State->lastReceivedSequenceFromServer;
			ByteWriter datagram;
			SerializeHeader(datagram, header);
			const SOCKET sendSocket = fromServer ? m_State->serverSocket : m_State->clientSocket;
			const sockaddr_in& toAddr = fromServer ? m_State->clientAddress : m_State->serverAddress;
			sendto(sendSocket, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&toAddr), sizeof(toAddr));
		};

		if (m_Initialized && m_State->sessionReady)
		{
			if ((currentTick - m_State->lastHeartbeatFromClientTick) >= kHeartbeatIntervalTicks)
			{
				sendHeartbeat(false);
				m_State->lastHeartbeatFromClientTick = currentTick;
			}
			if ((currentTick - m_State->lastHeartbeatFromServerTick) >= kHeartbeatIntervalTicks)
			{
				sendHeartbeat(true);
				m_State->lastHeartbeatFromServerTick = currentTick;
			}
		}

		std::array<std::uint8_t, kMaxPacketBytes> buffer{};
		auto pumpSocket = [&](SOCKET socketHandle, bool fromClient)
		{
			for (;;)
			{
				sockaddr_in fromAddr{};
				int fromLen = sizeof(fromAddr);
				const int recvBytes = recvfrom(socketHandle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
					reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
				if (recvBytes == SOCKET_ERROR)
				{
					const int error = WSAGetLastError();
					if (error == WSAEWOULDBLOCK)
						break;
					break;
				}
				if (recvBytes < static_cast<int>(kPacketHeaderSize))
					continue;

				ByteReader reader{ buffer.data(), static_cast<std::size_t>(recvBytes), 0 };
				PacketHeader header;
				if (!DeserializeHeader(reader, header) || header.magic != kPacketMagic)
					continue;

				if (fromClient)
					m_State->lastTrafficFromClientTick = currentTick;
				else
					m_State->lastTrafficFromServerTick = currentTick;

				if (header.sequence != 0u)
				{
					if (fromClient)
						m_State->lastReceivedSequenceFromClient = std::max(m_State->lastReceivedSequenceFromClient, header.sequence);
					else
						m_State->lastReceivedSequenceFromServer = std::max(m_State->lastReceivedSequenceFromServer, header.sequence);
				}
				processAck(fromClient, header.ack);
				if (static_cast<PacketType>(header.type) == PacketType::Heartbeat)
					continue;

				std::vector<std::uint8_t> payload(buffer.data() + static_cast<std::ptrdiff_t>(reader.offset), buffer.data() + recvBytes);
				enqueueFromDatagram(header, payload, fromClient);
			}
		};

		if (m_Initialized)
		{
			pumpSocket(m_State->serverSocket, true);
			pumpSocket(m_State->clientSocket, false);
		}

		if (m_State->sessionReady)
		{
			const bool serverTimedOut = (currentTick - m_State->lastTrafficFromServerTick) > kInactivityTimeoutTicks;
			const bool clientTimedOut = (currentTick - m_State->lastTrafficFromClientTick) > kInactivityTimeoutTicks;
			if (serverTimedOut || clientTimedOut)
			{
				m_State->sessionTimedOut = true;
				m_State->sessionReady = false;
				m_State->assignedClientID = 0u;
				SessionControlMessage disconnectMessage;
				disconnectMessage.type = SessionControlType::Disconnect;
				disconnectMessage.serverTick = currentTick;
				m_State->sessionControlToClient.push(disconnectMessage);
			}
		}
#endif

		UpdateSessionBootstrap();
	}

	void SocketNetTransport::Clear()
	{
		m_State->connectRequestsToServer = {};
		m_State->sessionControlToClient = {};
		m_State->inputToServer = {};
		m_State->snapshotToClient = {};
		m_State->delayedInputToServer.clear();
		m_State->delayedSnapshotToClient.clear();
		m_State->reliablePackets.clear();
		m_State->reassemblyFromClient.clear();
		m_State->reassemblyFromServer.clear();
		m_State->currentTick = 0u;
		m_State->assignedClientID = 0u;
		m_State->sessionReady = false;
		m_State->sessionTimedOut = false;
		m_State->hasOutstandingConnect = false;
		m_State->lastRequestedClientID = 0u;
		m_State->lastConnectRequestTick = 0u;
		m_State->lastRetryTick = 0u;
		m_State->connectRetryCount = 0u;
		m_State->nextSequenceClientToServer = 1u;
		m_State->nextSequenceServerToClient = 1u;
		m_State->lastReceivedSequenceFromClient = 0u;
		m_State->lastReceivedSequenceFromServer = 0u;
		m_State->nextFragmentGroup = 1u;
		m_State->lastTrafficFromClientTick = 0u;
		m_State->lastTrafficFromServerTick = 0u;
		m_State->lastHeartbeatFromClientTick = 0u;
		m_State->lastHeartbeatFromServerTick = 0u;
	}
}
