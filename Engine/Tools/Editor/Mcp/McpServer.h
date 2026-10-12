#pragma once

#include "Core.h"
#include "Platform/Socket.h"
#include "Containers/SPSCRingBuffer.h"
#include "Threading/ThreadTypes.h"

namespace tyr
{
	// The HTTP side of the editor's MCP server (MCP's streamable HTTP transport). A thread of its own
	// accepts connections on 127.0.0.1 and reads requests, and each JSON-RPC message posted to /mcp
	// is handed to the main thread, which replies to it. Anything else is answered on the thread.
	class McpServer final
	{
	public:
		McpServer();
		~McpServer();
		McpServer(const McpServer&) = delete;
		McpServer& operator=(const McpServer&) = delete;

		bool Start(uint16 port);
		void Stop();
		bool IsRunning() const { return m_Thread.joinable(); }

		// The next message posted, if any. Its body stays valid, with at least c_BodyPadding readable
		// bytes after it, until it's replied to. Every message must be replied to.
		bool TakeRequest(uint& outConnection, const char*& outBody, uint& outBodySize);
		// Replies with a JSON body, or with no body (202 Accepted) when json is null, such as to a
		// notification.
		void Reply(uint connection, const char* json, size_t size);

		// Lets a JSON parser read a little past the end of a body.
		static constexpr uint c_BodyPadding = 64;

	private:
		static constexpr uint c_MaxConnections = 8;
		static constexpr uint c_MaxRequestSize = 16 * 1024 * 1024;

		enum class ConnectionState : uint8
		{
			Free,
			// Reading a request.
			Reading,
			// Handed to the main thread, which owns the buffers until it replies.
			AwaitingReply,
			// The main thread wrote the reply into sendBuffer for this thread to send.
			ReplyReady
		};

		struct Connection
		{
			Socket socket;
			Atomic<ConnectionState> state{ ConnectionState::Free };
			Array<char> receiveBuffer;
			uint receivedSize = 0;
			// Where the request being replied to ends, so what came after it can be kept.
			uint requestEnd = 0;
			uint bodyOffset = 0;
			uint bodySize = 0;
			Array<char> sendBuffer;
			// Close once the reply is sent, as the client asked.
			bool closeAfterReply = false;
		};

		void Run();
		void AcceptConnection();
		// Reads what's arrived and handles every complete request in it.
		void ReadConnection(uint index);
		// True once a whole request is in, which is then handled. False if more is needed.
		bool HandleRequest(uint index);
		void SendReply(uint index);
		// Sends a reply this thread makes itself, with no body.
		void SendStatus(uint index, const char* status, const char* extraHeaders = "");
		void CloseConnection(uint index);

		Socket m_ListenSocket;
		Connection m_Connections[c_MaxConnections];
		// Indices of connections whose request is waiting for the main thread.
		SPSCRingBuffer<uint, 16> m_Requests;
		Thread m_Thread;
		Atomic<bool> m_StopRequested{ false };
		uint16 m_Port = 0;
	};
}
