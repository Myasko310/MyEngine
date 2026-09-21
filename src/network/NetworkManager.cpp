#include "network/NetworkManager.h"
#include <iostream>
#include <cstring>
#include <cerrno>

namespace MyEngine::Net
{
	// ============================================================
	// Socket Transport Implementation (minimal, Windows-only for now)
	// ============================================================

#ifdef _WIN32
	#include <winsock2.h>
	#pragma comment(lib, "ws2_32.lib")
	typedef int socklen_t;
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <fcntl.h>
	typedef int SOCKET;
	const int INVALID_SOCKET = -1;
	const int SOCKET_ERROR = -1;
	const int SHUT_RDWR = 2;
	#define closesocket close
#endif

	// Initialize Winsock (Windows only)
	static bool InitializeWinsock()
	{
#ifdef _WIN32
		static bool initialized = false;
		if (!initialized)
		{
			WSADATA wsaData;
			int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (result != 0)
			{
				std::cerr << "WSAStartup failed: " << result << std::endl;
				return false;
			}
			initialized = true;
		}
#endif
		return true;
	}

	static void PrintSocketError(const char* context)
	{
#ifdef _WIN32
		std::cerr << context << " failed: " << WSAGetLastError() << std::endl;
#else
		std::cerr << context << " failed" << std::endl;
#endif
	}

	// ============================================================
	// ClientConnection (stub for now)
	// ============================================================
	class ClientConnection
	{
	public:
		ClientConnection(const std::string& serverAddress, uint16_t serverPort)
			: m_ServerAddress(serverAddress), m_ServerPort(serverPort), m_Socket(INVALID_SOCKET), m_Connected(false)
		{
			InitializeWinsock();
			Connect();
		}

		~ClientConnection()
		{
			if (m_Socket != INVALID_SOCKET)
			{
				closesocket(m_Socket);
			}
		}

		bool IsConnected() const { return m_Connected; }

		void Connect()
		{
			m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (m_Socket == INVALID_SOCKET)
			{
				PrintSocketError("socket()");
				return;
			}

			struct sockaddr_in serverAddr;
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(m_ServerPort);
			serverAddr.sin_addr.s_addr = inet_addr(m_ServerAddress.c_str());

			if (connect(m_Socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
			{
				PrintSocketError("connect()");
				closesocket(m_Socket);
				m_Socket = INVALID_SOCKET;
				return;
			}

			m_Connected = true;
			std::cout << "Connected to server at " << m_ServerAddress << ":" << m_ServerPort << std::endl;
		}

		void SendPacket(const NetworkPacket& packet)
		{
			if (!m_Connected || m_Socket == INVALID_SOCKET)
				return;

			// Send packet (simplified; real implementation would include serialization)
			uint8_t buffer[1024];
			buffer[0] = static_cast<uint8_t>(packet.messageType);
			std::memcpy(buffer + 1, packet.data, packet.dataSize);

			if (send(m_Socket, (const char*)buffer, 1 + packet.dataSize, 0) == SOCKET_ERROR)
			{
				PrintSocketError("send()");
				m_Connected = false;
			}
		}

		bool ReceivePacket(NetworkPacket& packet)
		{
			if (!m_Connected || m_Socket == INVALID_SOCKET)
				return false;

			// Set non-blocking mode for receive
#ifdef _WIN32
			u_long mode = 1; // non-blocking
			ioctlsocket(m_Socket, FIONBIO, &mode);
#else
			int flags = fcntl(m_Socket, F_GETFL, 0);
			fcntl(m_Socket, F_SETFL, flags | O_NONBLOCK);
#endif

			uint8_t buffer[1024];
			int received = recv(m_Socket, (char*)buffer, sizeof(buffer), 0);

			if (received == SOCKET_ERROR)
			{
#ifdef _WIN32
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					PrintSocketError("recv()");
					m_Connected = false;
				}
#else
				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					PrintSocketError("recv()");
					m_Connected = false;
				}
#endif
				return false;
			}

			if (received == 0)
			{
				std::cerr << "Connection closed by server" << std::endl;
				m_Connected = false;
				return false;
			}

			packet.messageType = static_cast<MessageType>(buffer[0]);
			packet.dataSize = received - 1;
			std::memcpy(packet.data, buffer + 1, packet.dataSize);

			return true;
		}

	private:
		std::string m_ServerAddress;
		uint16_t m_ServerPort;
		SOCKET m_Socket;
		bool m_Connected;
	};

	// ============================================================
	// ServerConnection (stub for now)
	// ============================================================
	class ServerConnection
	{
	public:
		ServerConnection(uint16_t listenPort, int maxConnections)
			: m_ListenPort(listenPort), m_MaxConnections(maxConnections), m_ListenSocket(INVALID_SOCKET)
		{
			InitializeWinsock();
			StartListening();
		}

		~ServerConnection()
		{
			for (auto socket : m_ClientSockets)
			{
				closesocket(socket.second);
			}
			if (m_ListenSocket != INVALID_SOCKET)
			{
				closesocket(m_ListenSocket);
			}
		}

		void StartListening()
		{
			m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (m_ListenSocket == INVALID_SOCKET)
			{
				PrintSocketError("socket()");
				return;
			}

			struct sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = INADDR_ANY;
			addr.sin_port = htons(m_ListenPort);

			if (bind(m_ListenSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
			{
				PrintSocketError("bind()");
				closesocket(m_ListenSocket);
				m_ListenSocket = INVALID_SOCKET;
				return;
			}

			int listenResult = listen(m_ListenSocket, m_MaxConnections);
#ifdef _WIN32
			if (listenResult == SOCKET_ERROR)
#else
			if (listenResult < 0)
#endif
			{
				PrintSocketError("listen()");
				closesocket(m_ListenSocket);
				m_ListenSocket = INVALID_SOCKET;
				return;
			}

			std::cout << "Server listening on port " << m_ListenPort << std::endl;
		}

		void Update()
		{
			if (m_ListenSocket == INVALID_SOCKET)
				return;

			// Set non-blocking mode
#ifdef _WIN32
			u_long mode = 1;
			ioctlsocket(m_ListenSocket, FIONBIO, &mode);
#else
			int flags = fcntl(m_ListenSocket, F_GETFL, 0);
			fcntl(m_ListenSocket, F_SETFL, flags | O_NONBLOCK);
#endif

			struct sockaddr_in clientAddr;
			socklen_t clientAddrLen = sizeof(clientAddr);

			SOCKET clientSocket = accept(m_ListenSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
			if (clientSocket != INVALID_SOCKET)
			{
				uint32_t clientID = static_cast<uint32_t>(m_ClientSockets.size() + 1);
				m_ClientSockets[clientID] = clientSocket;
				std::cout << "Client " << clientID << " connected from " << inet_ntoa(clientAddr.sin_addr) << std::endl;
			}
		}

		void BroadcastPacket(const NetworkPacket& packet)
		{
			for (auto& [clientID, socket] : m_ClientSockets)
			{
				SendPacketToClient(clientID, packet);
			}
		}

		void SendPacketToClient(uint32_t clientID, const NetworkPacket& packet)
		{
			auto it = m_ClientSockets.find(clientID);
			if (it == m_ClientSockets.end())
				return;

			SOCKET socket = it->second;

			uint8_t buffer[1024];
			buffer[0] = static_cast<uint8_t>(packet.messageType);
			std::memcpy(buffer + 1, packet.data, packet.dataSize);

			if (send(socket, (const char*)buffer, 1 + packet.dataSize, 0) == SOCKET_ERROR)
			{
				std::cerr << "send() to client " << clientID << " failed" << std::endl;
				closesocket(socket);
				m_ClientSockets.erase(clientID);
			}
		}

		bool ReceivePacketFromClient(uint32_t clientID, NetworkPacket& packet)
		{
			auto it = m_ClientSockets.find(clientID);
			if (it == m_ClientSockets.end())
				return false;

			SOCKET socket = it->second;

#ifdef _WIN32
			u_long mode = 1; // non-blocking
			ioctlsocket(socket, FIONBIO, &mode);
#else
			int flags = fcntl(socket, F_GETFL, 0);
			fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif

			uint8_t buffer[1024];
			int received = recv(socket, (char*)buffer, sizeof(buffer), 0);

			if (received == SOCKET_ERROR)
			{
#ifdef _WIN32
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					std::cerr << "recv() from client " << clientID << " failed" << std::endl;
					closesocket(socket);
					m_ClientSockets.erase(clientID);
				}
#else
				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					std::cerr << "recv() from client " << clientID << " failed" << std::endl;
					closesocket(socket);
					m_ClientSockets.erase(clientID);
				}
#endif
				return false;
			}

			if (received == 0)
			{
				std::cout << "Client " << clientID << " disconnected" << std::endl;
				closesocket(socket);
				m_ClientSockets.erase(clientID);
				return false;
			}

			packet.messageType = static_cast<MessageType>(buffer[0]);
			packet.dataSize = received - 1;
			std::memcpy(packet.data, buffer + 1, packet.dataSize);

			return true;
		}

		const std::unordered_map<uint32_t, SOCKET>& GetClientSockets() const
		{
			return m_ClientSockets;
		}

	private:
		uint16_t m_ListenPort;
		int m_MaxConnections;
		SOCKET m_ListenSocket;
		std::unordered_map<uint32_t, SOCKET> m_ClientSockets;
	};

	// ============================================================
	// NetworkManager Implementation
	// ============================================================

	NetworkManager::NetworkManager(bool isServer, uint32_t clientID)
		: m_IsServer(isServer), m_LocalClientID(clientID)
	{
	}

	NetworkManager::~NetworkManager() = default;

	std::shared_ptr<NetworkManager> NetworkManager::CreateClient(const std::string& serverAddress, uint16_t serverPort)
	{
		auto manager = std::make_shared<NetworkManager>(false, 0);
		manager->m_ClientConnection = std::make_unique<ClientConnection>(serverAddress, serverPort);
		manager->m_IsConnected = manager->m_ClientConnection->IsConnected();
		return manager;
	}

	std::shared_ptr<NetworkManager> NetworkManager::CreateServer(uint16_t listenPort, int maxConnections)
	{
		auto manager = std::make_shared<NetworkManager>(true, 0); // Server has no single client ID
		manager->m_ServerConnection = std::make_unique<ServerConnection>(listenPort, maxConnections);
		manager->m_IsConnected = true; // Server is "connected" as soon as it's listening
		return manager;
	}

	void NetworkManager::Update()
	{
		m_PacketsSentThisFrame = 0;
		m_PacketsReceivedThisFrame = 0;

		if (m_IsServer)
		{
			m_ServerConnection->Update();
			ProcessServerMessages();
		}
		else
		{
			ProcessClientMessages();
		}
	}

	void NetworkManager::SendInputCommand(const InputCommand& input)
	{
		if (!m_ClientConnection || !m_IsConnected)
			return;

		NetworkPacket packet(MessageType::ClientInput);
		// Simplified serialization; real version would use proper serialization
		std::memcpy(packet.data, &input, sizeof(input));
		packet.dataSize = sizeof(input);

		m_ClientConnection->SendPacket(packet);
		m_PacketsSentThisFrame++;
	}

	void NetworkManager::BroadcastEntityState(const EntityState& state)
	{
		if (!m_ServerConnection || !m_IsServer)
			return;

		NetworkPacket packet(MessageType::ServerStateUpdate);
		std::memcpy(packet.data, &state, sizeof(state));
		packet.dataSize = sizeof(state);

		m_ServerConnection->BroadcastPacket(packet);
		m_PacketsSentThisFrame++;
	}

	const std::vector<uint32_t>& NetworkManager::GetConnectedClientIDs() const
	{
		return m_ConnectedClientIDs;
	}

	void NetworkManager::SetInputCommandCallback(InputCommandCallback callback)
	{
		m_InputCommandCallback = callback;
	}

	void NetworkManager::SetEntityStateCallback(EntityStateCallback callback)
	{
		m_EntityStateCallback = callback;
	}

	void NetworkManager::ProcessClientMessages()
	{
		if (!m_ClientConnection)
			return;

		NetworkPacket packet;
		while (m_ClientConnection->ReceivePacket(packet))
		{
			m_PacketsReceivedThisFrame++;

			switch (packet.messageType)
			{
			case MessageType::ServerStateUpdate:
			{
				EntityState state;
				std::memcpy(&state, packet.data, sizeof(state));
				if (m_EntityStateCallback)
					m_EntityStateCallback(state);
				break;
			}
			case MessageType::ServerAcceptConnection:
			{
				// Extract clientID from packet
				if (packet.dataSize >= sizeof(uint32_t))
				{
					std::memcpy(&m_LocalClientID, packet.data, sizeof(uint32_t));
					std::cout << "Client accepted with ID: " << m_LocalClientID << std::endl;
				}
				break;
			}
			default:
				break;
			}
		}
	}

	void NetworkManager::ProcessServerMessages()
	{
		if (!m_ServerConnection)
			return;

		m_ConnectedClientIDs.clear();
		for (const auto& [clientID, _] : m_ServerConnection->GetClientSockets())
		{
			m_ConnectedClientIDs.push_back(clientID);

			NetworkPacket packet;
			if (m_ServerConnection->ReceivePacketFromClient(clientID, packet))
			{
				m_PacketsReceivedThisFrame++;

				switch (packet.messageType)
				{
				case MessageType::ClientInput:
				{
					InputCommand input;
					std::memcpy(&input, packet.data, sizeof(input));
					input.clientID = clientID;
					if (m_InputCommandCallback)
						m_InputCommandCallback(clientID, input);
					break;
				}
				default:
					break;
				}
			}
		}
	}
}
