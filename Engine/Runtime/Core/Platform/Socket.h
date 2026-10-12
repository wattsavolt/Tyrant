#pragma once

#include "CoreMacros.h"
#include "Base/Base.h"

namespace tyr
{
	// A blocking TCP socket over IPv4. The OS networking it needs starts with the first socket opened
	// and stops when the last one closes.
	class TYR_CORE_API Socket final
	{
	public:
		Socket() = default;
		~Socket();
		Socket(const Socket&) = delete;
		Socket& operator=(const Socket&) = delete;

		// Listens for connections on an address, such as "127.0.0.1" to only allow this machine.
		bool Listen(const char* address, uint16 port);
		// Takes the next connection waiting on a listening socket, waiting for one if there isn't any.
		bool Accept(Socket& outConnection) const;
		// Bytes received, 0 once the other end has closed, or -1 on failure. Waits for at least one byte.
		int Receive(void* buffer, uint size) const;
		// Sends all of it, waiting as needed. False on failure.
		bool Send(const void* data, size_t size) const;
		void Close();
		bool IsOpen() const { return m_Handle != c_InvalidHandle; }

		// Waits up to timeoutMs for any of the sockets to have data to read, a connection to accept or
		// a close to notice, and sets each one's entry in outReady. False on failure.
		static bool WaitForReady(const Socket* const* sockets, uint count, uint timeoutMs, bool* outReady);

		static constexpr uint c_MaxWaitSockets = 32;

	private:
		static constexpr uint64 c_InvalidHandle = ~uint64(0);

		uint64 m_Handle = c_InvalidHandle;
	};
}
