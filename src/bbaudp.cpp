/*

# The Broadband Adapter's network

The host side of `bba.cpp`: the frames the emulated adapter transmits go out as UDP datagrams, and
the datagrams it receives are handed back to it as frames. Two emulators (or an emulator and a
small bridge to a real segment) therefore see each other's Ethernet traffic, which is what a LAN
game needs.

This is a **front end** module, the same way `padsdl.cpp` is: the device never opens a socket, it
hands frames to a `BbaBackend` (see bba.h), and the build supplies the implementation. The headless
build compiles this file too, because the headless emulator is exactly the one a scripted LAN test
runs; a build with no configuration for a peer gets no backend at all and the adapter is a console
with the cable unplugged.

## What the configuration says

| Setting | Meaning |
|---|---|
| `BBA_PEER` | `host:port` of the other end. Empty (the default) means "no network". |
| `BBA_PORT` | the UDP port this emulator listens on. |

One instance of a two-console LAN therefore runs with `BBA_PEER=127.0.0.1:20001 BBA_PORT=20002`
and the other with the two values the other way round.

*/

#include "pch.h"
#include "bba.h"

using namespace Debug;

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET bba_socket_t;
#define BBA_INVALID_SOCKET  INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
typedef int bba_socket_t;
#define BBA_INVALID_SOCKET  (-1)
#endif

namespace
{
	//! How many received frames are held before the oldest is dropped. A LAN game's traffic is a
	//! burst per frame, so a small queue is enough; a console that stops reading must not grow the
	//! emulator's memory.
	const size_t BbaRxQueue = 64;

	class BbaUdpBackend : public BbaBackend
	{
		bba_socket_t sock = BBA_INVALID_SOCKET;
		sockaddr_in  peer = {};
		bool         havePeer = false;
		bool         linkUp = false;

		std::deque<std::vector<uint8_t>> queue;

		//! Take everything the socket has and put it in the queue.
		void Pump()
		{
			if (sock == BBA_INVALID_SOCKET)
			{
				return;
			}

			for (int guard = 0; guard < 64; guard++)
			{
				std::vector<uint8_t> frame(2048);

				int received = (int)recvfrom(sock, (char*)frame.data(), (int)frame.size(), 0, nullptr, nullptr);

				if (received <= 0)
				{
					return;
				}

				frame.resize((size_t)received);

				if (queue.size() >= BbaRxQueue)
				{
					queue.pop_front();
				}

				queue.push_back(std::move(frame));
			}
		}

	public:
		BbaUdpBackend() {}

		~BbaUdpBackend() override
		{
#ifdef _WIN32
			if (sock != BBA_INVALID_SOCKET)
			{
				closesocket(sock);
			}

			WSACleanup();
#else
			if (sock != BBA_INVALID_SOCKET)
			{
				close(sock);
			}
#endif
		}

		//! Open the socket and remember where the other end is. Answers false (and leaves the object
		//! unusable) when the host has no sockets to give.
		bool Open(const char* peerSpec, int localPort)
		{
			std::string spec = peerSpec != nullptr ? peerSpec : "";
			std::string host = spec;
			int port = 0;

			size_t colon = spec.rfind(':');

			if (colon != std::string::npos)
			{
				host = spec.substr(0, colon);
				port = atoi(spec.c_str() + colon + 1);
			}

			if (host.empty() || port <= 0)
			{
				return false;
			}

#ifdef _WIN32
			WSADATA data;

			if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			{
				Report(Channel::EXI, "BBA network: Winsock could not be started\n");
				return false;
			}
#endif

			sock = socket(AF_INET, SOCK_DGRAM, 0);

			if (sock == BBA_INVALID_SOCKET)
			{
				Report(Channel::EXI, "BBA network: no UDP socket\n");
				return false;
			}

			// Non-blocking: the emulation thread polls the socket while it runs, and it must never
			// wait for the host.
#ifdef _WIN32
			u_long mode = 1;
			ioctlsocket(sock, FIONBIO, &mode);
#else
			int flags = fcntl(sock, F_GETFL, 0);
			fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

			// Bind to the local port so the other end can answer (and so that two frames do not
			// loop back into this instance).
			sockaddr_in local = {};
			local.sin_family = AF_INET;
			local.sin_addr.s_addr = htonl(INADDR_ANY);
			local.sin_port = htons((unsigned short)localPort);

			if (bind(sock, (sockaddr*)&local, sizeof(local)) != 0)
			{
				Report(Channel::EXI, "BBA network: UDP port %i is taken\n", localPort);
				return false;
			}

			peer.sin_family = AF_INET;
			peer.sin_port = htons((unsigned short)port);

			if (inet_pton(AF_INET, host.c_str(), &peer.sin_addr) != 1)
			{
				Report(Channel::EXI, "BBA network: %s is not an address\n", host.c_str());
				return false;
			}

			havePeer = true;
			linkUp = true;

			Report(Channel::EXI, "BBA network: frames go to %s:%i, listening on %i\n",
				host.c_str(), port, localPort);

			return true;
		}

		// -----------------------------------------------------------------------

		void Transmit(const uint8_t* frame, size_t length) override
		{
			if (!havePeer || sock == BBA_INVALID_SOCKET || length == 0)
			{
				return;
			}

			sendto(sock, (const char*)frame, (int)length, 0, (const sockaddr*)&peer, sizeof(peer));
		}

		bool HasReceive() override
		{
			Pump();
			return !queue.empty();
		}

		bool Receive(uint8_t* frame, size_t capacity, size_t* length) override
		{
			Pump();

			if (queue.empty())
			{
				return false;
			}

			std::vector<uint8_t>& next = queue.front();
			size_t take = next.size() < capacity ? next.size() : capacity;

			memcpy(frame, next.data(), take);

			if (length != nullptr)
			{
				*length = take;
			}

			queue.pop_front();
			return true;
		}

		bool LinkUp() override
		{
			return linkUp;
		}
	};
}

// ---------------------------------------------------------------------------
// The front end's entry point

BbaBackend* BbaNetworkCreate()
{
	const wchar_t* peer = GetConfigString(USER_BBA_PEER, USER_HW);
	int port = GetConfigInt(USER_BBA_PORT, USER_HW);

	std::string spec = Util::WstringToString(peer);

	if (spec.empty())
	{
		// No peer in the configuration: the adapter is present and the network is not. That is the
		// state a user who has not set anything up should be in.
		return nullptr;
	}

	if (port <= 0 || port > 65535)
	{
		port = 20001;
	}

	BbaUdpBackend* backend = new BbaUdpBackend();

	if (!backend->Open(spec.c_str(), port))
	{
		delete backend;
		return nullptr;
	}

	return backend;
}
