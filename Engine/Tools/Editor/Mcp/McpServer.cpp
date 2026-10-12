#include "McpServer.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace tyr
{
	namespace
	{
		constexpr uint c_InitialBufferSize = 64 * 1024;
		// How long the thread waits for sockets before checking for replies to send.
		constexpr uint c_PollIntervalMs = 5;
		constexpr uint c_MaxHeaderSize = 64 * 1024;

		bool StartsWithIgnoreCase(const char* text, size_t textLength, const char* prefix)
		{
			const size_t prefixLength = strlen(prefix);
			if (textLength < prefixLength)
			{
				return false;
			}
			for (size_t i = 0; i < prefixLength; ++i)
			{
				char c = text[i];
				c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
				if (c != prefix[i])
				{
					return false;
				}
			}
			return true;
		}

		// Where the line starting at text ends, or end if it doesn't before then.
		const char* FindLineEnd(const char* text, const char* end)
		{
			for (const char* c = text; c + 1 < end; ++c)
			{
				if (c[0] == '\r' && c[1] == '\n')
				{
					return c;
				}
			}
			return end;
		}

		const char* FindHeaderEnd(const char* data, uint size)
		{
			for (uint i = 0; i + 3 < size; ++i)
			{
				if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n')
				{
					return data + i;
				}
			}
			return nullptr;
		}

		// Only pages served from this machine may call the server, so a website can't reach it
		// through the browser. Clients other than browsers don't send an origin at all.
		bool IsLocalOrigin(const char* origin, size_t length)
		{
			const char* const c_LocalOrigins[] = { "http://localhost", "http://127.0.0.1", "http://[::1]" };
			for (const char* localOrigin : c_LocalOrigins)
			{
				const size_t localLength = strlen(localOrigin);
				if (length >= localLength && strncmp(origin, localOrigin, localLength) == 0 && (length == localLength || origin[localLength] == ':'))
				{
					return true;
				}
			}
			return false;
		}
	}

	McpServer::McpServer()
	{
		for (Connection& connection : m_Connections)
		{
			connection.receiveBuffer.Resize(c_InitialBufferSize);
		}
	}

	McpServer::~McpServer()
	{
		Stop();
	}

	bool McpServer::Start(uint16 port)
	{
		if (IsRunning() || !m_ListenSocket.Listen("127.0.0.1", port))
		{
			return false;
		}
		m_Port = port;
		m_StopRequested = false;
		m_Thread = Thread(&McpServer::Run, this);
		return true;
	}

	void McpServer::Stop()
	{
		if (!IsRunning())
		{
			return;
		}
		m_StopRequested = true;
		m_Thread.join();

		for (uint i = 0; i < c_MaxConnections; ++i)
		{
			m_Connections[i].state = ConnectionState::Reading;
			CloseConnection(i);
		}
		m_ListenSocket.Close();
		while (m_Requests.Dequeue())
		{
		}
	}

	bool McpServer::TakeRequest(uint& outConnection, const char*& outBody, uint& outBodySize)
	{
		const Optional<uint> index = m_Requests.Dequeue();
		if (!index)
		{
			return false;
		}
		const Connection& connection = m_Connections[index.value()];
		outConnection = index.value();
		outBody = connection.receiveBuffer.Data() + connection.bodyOffset;
		outBodySize = connection.bodySize;
		return true;
	}

	void McpServer::Reply(uint index, const char* json, size_t size)
	{
		Connection& connection = m_Connections[index];
		TYR_ASSERT(connection.state == ConnectionState::AwaitingReply);

		char header[256];
		const int headerLength = json
			? snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n", size)
			: snprintf(header, sizeof(header), "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n");

		connection.sendBuffer.Resize((uint)headerLength + (json ? (uint)size : 0u));
		memcpy(connection.sendBuffer.Data(), header, (size_t)headerLength);
		if (json)
		{
			memcpy(connection.sendBuffer.Data() + headerLength, json, size);
		}
		// Hands the buffers back to the network thread.
		connection.state = ConnectionState::ReplyReady;
	}

	void McpServer::Run()
	{
		while (!m_StopRequested)
		{
			// The listener and every connection reading a request.
			const Socket* sockets[c_MaxConnections + 1];
			uint indices[c_MaxConnections + 1];
			uint count = 0;
			sockets[count] = &m_ListenSocket;
			indices[count++] = c_MaxConnections;
			for (uint i = 0; i < c_MaxConnections; ++i)
			{
				if (m_Connections[i].state == ConnectionState::Reading)
				{
					sockets[count] = &m_Connections[i].socket;
					indices[count++] = i;
				}
			}

			bool ready[c_MaxConnections + 1];
			if (!Socket::WaitForReady(sockets, count, c_PollIntervalMs, ready))
			{
				TYR_THREAD_SLEEP_MS(c_PollIntervalMs);
				continue;
			}

			for (uint i = 0; i < count; ++i)
			{
				if (!ready[i])
				{
					continue;
				}
				if (indices[i] == c_MaxConnections)
				{
					AcceptConnection();
				}
				else
				{
					ReadConnection(indices[i]);
				}
			}

			for (uint i = 0; i < c_MaxConnections; ++i)
			{
				if (m_Connections[i].state == ConnectionState::ReplyReady)
				{
					SendReply(i);
				}
			}
		}
	}

	void McpServer::AcceptConnection()
	{
		for (uint i = 0; i < c_MaxConnections; ++i)
		{
			Connection& connection = m_Connections[i];
			if (connection.state == ConnectionState::Free)
			{
				if (!m_ListenSocket.Accept(connection.socket))
				{
					return;
				}
				connection.receivedSize = 0;
				connection.closeAfterReply = false;
				connection.state = ConnectionState::Reading;
				return;
			}
		}

		// Every connection is in use, so this one is turned away.
		Socket rejected;
		if (m_ListenSocket.Accept(rejected))
		{
			const char c_Busy[] = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
			rejected.Send(c_Busy, sizeof(c_Busy) - 1);
		}
	}

	void McpServer::ReadConnection(uint index)
	{
		Connection& connection = m_Connections[index];
		Array<char>& buffer = connection.receiveBuffer;
		if (connection.receivedSize + c_BodyPadding + 1024 > buffer.Size())
		{
			buffer.Resize(buffer.Size() * 2);
		}

		const int received = connection.socket.Receive(buffer.Data() + connection.receivedSize, buffer.Size() - connection.receivedSize - c_BodyPadding);
		if (received <= 0)
		{
			CloseConnection(index);
			return;
		}
		connection.receivedSize += (uint)received;

		while (connection.state == ConnectionState::Reading && HandleRequest(index))
		{
		}
	}

	bool McpServer::HandleRequest(uint index)
	{
		Connection& connection = m_Connections[index];
		const char* data = connection.receiveBuffer.Data();
		const char* headerEnd = FindHeaderEnd(data, connection.receivedSize);
		if (!headerEnd)
		{
			if (connection.receivedSize > c_MaxHeaderSize)
			{
				connection.closeAfterReply = true;
				SendStatus(index, "431 Request Header Fields Too Large");
			}
			return false;
		}

		// The request line, "METHOD /path HTTP/1.1".
		const char* lineEnd = FindLineEnd(data, headerEnd + 2);
		const char* methodEnd = static_cast<const char*>(memchr(data, ' ', (size_t)(lineEnd - data)));
		const char* pathStart = methodEnd ? methodEnd + 1 : lineEnd;
		const char* pathEnd = static_cast<const char*>(memchr(pathStart, ' ', (size_t)(lineEnd - pathStart)));
		pathEnd = pathEnd ? pathEnd : lineEnd;
		const char* query = static_cast<const char*>(memchr(pathStart, '?', (size_t)(pathEnd - pathStart)));
		const size_t pathLength = (size_t)((query ? query : pathEnd) - pathStart);
		const size_t methodLength = methodEnd ? (size_t)(methodEnd - data) : 0;

		size_t contentLength = 0;
		bool localOrigin = true;
		connection.closeAfterReply = false;
		for (const char* line = lineEnd + 2; line < headerEnd;)
		{
			const char* next = FindLineEnd(line, headerEnd + 2);
			const size_t lineLength = (size_t)(next - line);
			const char* colon = static_cast<const char*>(memchr(line, ':', lineLength));
			if (colon)
			{
				const char* value = colon + 1;
				while (*value == ' ' || *value == '\t')
				{
					++value;
				}
				const size_t valueLength = (size_t)(next - value);
				const size_t nameLength = (size_t)(colon - line);
				if (nameLength == 14 && StartsWithIgnoreCase(line, nameLength, "content-length"))
				{
					contentLength = strtoull(value, nullptr, 10);
				}
				else if (nameLength == 6 && StartsWithIgnoreCase(line, nameLength, "origin"))
				{
					localOrigin = IsLocalOrigin(value, valueLength);
				}
				else if (nameLength == 10 && StartsWithIgnoreCase(line, nameLength, "connection"))
				{
					connection.closeAfterReply = StartsWithIgnoreCase(value, valueLength, "close");
				}
			}
			line = next + 2;
		}

		const uint bodyOffset = (uint)(headerEnd + 4 - data);
		if (contentLength > c_MaxRequestSize)
		{
			connection.closeAfterReply = true;
			SendStatus(index, "413 Content Too Large");
			return false;
		}

		// Waits for the whole body, with room for the padding after it.
		const uint requestEnd = bodyOffset + (uint)contentLength;
		if (connection.receivedSize < requestEnd)
		{
			if (requestEnd + c_BodyPadding > connection.receiveBuffer.Size())
			{
				connection.receiveBuffer.Resize(requestEnd + c_BodyPadding);
			}
			return false;
		}
		connection.requestEnd = requestEnd;

		if (pathLength != 4 || strncmp(pathStart, "/mcp", 4) != 0)
		{
			SendStatus(index, "404 Not Found");
			return connection.state == ConnectionState::Reading;
		}
		if (!localOrigin)
		{
			SendStatus(index, "403 Forbidden");
			return connection.state == ConnectionState::Reading;
		}
		// No server-sent event stream or sessions, so only posted messages are taken.
		if (methodLength != 4 || strncmp(data, "POST", 4) != 0)
		{
			SendStatus(index, "405 Method Not Allowed", "Allow: POST\r\n");
			return connection.state == ConnectionState::Reading;
		}

		connection.bodyOffset = bodyOffset;
		connection.bodySize = (uint)contentLength;
		connection.state = ConnectionState::AwaitingReply;
		m_Requests.Enqueue(index);
		return false;
	}

	void McpServer::SendReply(uint index)
	{
		Connection& connection = m_Connections[index];
		const bool sent = connection.socket.Send(connection.sendBuffer.Data(), connection.sendBuffer.Size());

		// Keeps anything that arrived after the request, such as the start of the next one.
		const uint remaining = connection.receivedSize - connection.requestEnd;
		memmove(connection.receiveBuffer.Data(), connection.receiveBuffer.Data() + connection.requestEnd, remaining);
		connection.receivedSize = remaining;
		connection.state = ConnectionState::Reading;

		if (!sent || connection.closeAfterReply)
		{
			CloseConnection(index);
			return;
		}
		while (connection.state == ConnectionState::Reading && HandleRequest(index))
		{
		}
	}

	void McpServer::SendStatus(uint index, const char* status, const char* extraHeaders)
	{
		Connection& connection = m_Connections[index];
		char response[256];
		const int length = snprintf(response, sizeof(response), "HTTP/1.1 %s\r\nContent-Length: 0\r\n%s\r\n", status, extraHeaders);
		const bool sent = connection.socket.Send(response, (size_t)length);

		const uint requestEnd = connection.requestEnd <= connection.receivedSize ? connection.requestEnd : connection.receivedSize;
		memmove(connection.receiveBuffer.Data(), connection.receiveBuffer.Data() + requestEnd, connection.receivedSize - requestEnd);
		connection.receivedSize -= requestEnd;
		connection.requestEnd = 0;

		if (!sent || connection.closeAfterReply)
		{
			CloseConnection(index);
		}
	}

	void McpServer::CloseConnection(uint index)
	{
		Connection& connection = m_Connections[index];
		connection.socket.Close();
		connection.receivedSize = 0;
		connection.requestEnd = 0;
		connection.closeAfterReply = false;
		connection.state = ConnectionState::Free;
	}
}
