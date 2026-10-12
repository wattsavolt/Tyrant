// Winsock has to come before anything that pulls in windows.h.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Platform/Socket.h"
#include "Threading/ThreadTypes.h"
#include "Utility/Utility.h"

namespace tyr
{
	namespace
	{
		Mutex s_NetworkingMutex;
		uint s_OpenSocketCount = 0;

		bool AcquireNetworking()
		{
			LockGuard lock(s_NetworkingMutex);
			if (s_OpenSocketCount == 0)
			{
				WSADATA data;
				if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
				{
					TYR_LOG_ERROR("Failed to start Windows networking.");
					return false;
				}
			}
			++s_OpenSocketCount;
			return true;
		}

		void ReleaseNetworking()
		{
			LockGuard lock(s_NetworkingMutex);
			if (--s_OpenSocketCount == 0)
			{
				WSACleanup();
			}
		}
	}

	Socket::~Socket()
	{
		Close();
	}

	bool Socket::Listen(const char* address, uint16 port)
	{
		Close();
		if (!AcquireNetworking())
		{
			return false;
		}

		const SOCKET handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (handle == INVALID_SOCKET)
		{
			ReleaseNetworking();
			return false;
		}
		m_Handle = static_cast<uint64>(handle);

		sockaddr_in socketAddress{};
		socketAddress.sin_family = AF_INET;
		socketAddress.sin_port = htons(port);
		if (inet_pton(AF_INET, address, &socketAddress.sin_addr) != 1
			|| bind(handle, reinterpret_cast<const sockaddr*>(&socketAddress), sizeof(socketAddress)) == SOCKET_ERROR
			|| listen(handle, SOMAXCONN) == SOCKET_ERROR)
		{
			TYR_LOG_ERROR("Failed to listen on %s:%u (error %d).", address, static_cast<uint>(port), WSAGetLastError());
			Close();
			return false;
		}
		return true;
	}

	bool Socket::Accept(Socket& outConnection) const
	{
		const SOCKET handle = accept(static_cast<SOCKET>(m_Handle), nullptr, nullptr);
		if (handle == INVALID_SOCKET || !AcquireNetworking())
		{
			if (handle != INVALID_SOCKET)
			{
				closesocket(handle);
			}
			return false;
		}

		outConnection.Close();
		outConnection.m_Handle = static_cast<uint64>(handle);
		// Replies are written whole, so there's nothing to gain from waiting to batch them.
		const BOOL noDelay = TRUE;
		setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
		return true;
	}

	int Socket::Receive(void* buffer, uint size) const
	{
		const int received = recv(static_cast<SOCKET>(m_Handle), static_cast<char*>(buffer), static_cast<int>(size), 0);
		return received == SOCKET_ERROR ? -1 : received;
	}

	bool Socket::Send(const void* data, size_t size) const
	{
		const char* bytes = static_cast<const char*>(data);
		while (size > 0)
		{
			const int chunk = static_cast<int>(size < INT_MAX ? size : INT_MAX);
			const int sent = send(static_cast<SOCKET>(m_Handle), bytes, chunk, 0);
			if (sent == SOCKET_ERROR)
			{
				return false;
			}
			bytes += sent;
			size -= static_cast<size_t>(sent);
		}
		return true;
	}

	void Socket::Close()
	{
		if (!IsOpen())
		{
			return;
		}
		closesocket(static_cast<SOCKET>(m_Handle));
		m_Handle = c_InvalidHandle;
		ReleaseNetworking();
	}

	bool Socket::WaitForReady(const Socket* const* sockets, uint count, uint timeoutMs, bool* outReady)
	{
		TYR_ASSERT(count <= c_MaxWaitSockets);
		WSAPOLLFD pollFds[c_MaxWaitSockets];
		for (uint i = 0; i < count; ++i)
		{
			pollFds[i].fd = static_cast<SOCKET>(sockets[i]->m_Handle);
			pollFds[i].events = POLLRDNORM;
			pollFds[i].revents = 0;
		}

		const int result = WSAPoll(pollFds, count, static_cast<INT>(timeoutMs));
		for (uint i = 0; i < count; ++i)
		{
			outReady[i] = result > 0 && pollFds[i].revents != 0;
		}
		return result != SOCKET_ERROR;
	}
}
