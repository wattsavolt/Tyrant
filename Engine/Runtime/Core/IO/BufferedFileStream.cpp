#include "BufferedFileStream.h"
#include "Platform/Platform.h"

namespace tyr
{
	BufferedFileStream::BufferedFileStream(uint8* buffer, size_t bufferSize, const char* filePath, Operation op, bool overwrite)
		: FileStream(filePath, op, overwrite)
		, m_Buffer(buffer)
		, m_BufferSize(bufferSize)
		, m_BufferOffset(0)
		, m_MemoryRead(0)
	{
		if (op == Operation::Read)
		{
			m_MemoryRead = FileStream::Read(m_Buffer, bufferSize);
		}
	}

	BufferedFileStream::~BufferedFileStream()
	{
		if (m_Operation == Operation::Write)
		{
			const size_t bytesToWrite = m_BufferOffset;
			if (bytesToWrite > 0)
			{
				FileStream::Write(m_Buffer, bytesToWrite);
			}
		}
	}

	size_t BufferedFileStream::Write(const void* buffer, size_t count)
	{
		TYR_ASSERT(buffer && m_Operation == Operation::Write);
		const size_t bufferSizeRemaining = m_BufferSize - m_BufferOffset;
		if (count >= bufferSizeRemaining)
		{
			memcpy(&m_Buffer[m_BufferOffset], buffer, bufferSizeRemaining);
			FileStream::Write(m_Buffer, m_BufferSize);
			const size_t remaining = count - bufferSizeRemaining;
			memcpy(&m_Buffer[0], buffer, remaining);
			m_BufferOffset = remaining;
		}
		else
		{
			memcpy(&m_Buffer[m_BufferOffset], buffer, count);
			m_BufferOffset += count;
		}
		return count;
	}

	size_t BufferedFileStream::Read(void* buffer, size_t count) 
	{
		TYR_ASSERT(buffer && m_Operation == Operation::Read);
		const size_t memoryRemaining = m_MemoryRead - m_BufferOffset;
		if (count >= memoryRemaining)
		{
			const size_t existingBytesToReadFromBuffer = m_MemoryRead - memoryRemaining;
			memcpy(buffer, &m_Buffer[0], existingBytesToReadFromBuffer);
			m_MemoryRead = FileStream::Read(m_Buffer, m_BufferSize);
			const size_t remaining = count - existingBytesToReadFromBuffer;
			TYR_ASSERT(m_MemoryRead >= remaining);
			memcpy(buffer, &m_Buffer[0], remaining);
			m_BufferOffset = remaining;
		}
		else
		{
			memcpy(buffer, &m_Buffer[0], count);
			m_BufferOffset += count;
		}
		return count;
	}

	BinaryStream::Type BufferedFileStream::GetStreamType() const
	{
		return BinaryStream::Type::File;
	}
}