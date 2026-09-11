#include "BufferedFileStream.h"
#include "Platform/Platform.h"

namespace tyr
{
	BufferedFileStream::BufferedFileStream(uint8* buffer, size_t bufferSize, const char* filePath, Operation op, bool overwrite)
		: FileStream(filePath, op, overwrite)
		, m_Buffer(buffer)
		, m_BufferSize(bufferSize)
		, m_BufferOffset(0)
	{
		if (op == Operation::Read)
		{
			m_MemoryRead = FileStream::Read(m_Buffer, bufferSize);
		}
		else
		{
			m_MemoryRead = 0;
		}
	}

	BufferedFileStream::~BufferedFileStream()
	{
		if (m_Operation == Operation::Write)
		{
			if (m_BufferOffset > 0)
			{
				FileStream::Write(m_Buffer, m_BufferOffset);
			}
		}
	}

	size_t BufferedFileStream::Write(const void* buffer, size_t count)
	{
		TYR_ASSERT(buffer && m_Operation == Operation::Write && count > 0);
		const size_t bufferSizeRemaining = m_BufferSize - m_BufferOffset;
		if (count > bufferSizeRemaining)
		{
			memcpy(&m_Buffer[m_BufferOffset], buffer, bufferSizeRemaining);
			FileStream::Write(m_Buffer, m_BufferSize);
			const size_t remaining = count - bufferSizeRemaining;
			const uint8* remainingData = &((const uint8*)buffer)[bufferSizeRemaining];
			// Handle the case of a size that is bigger than the buffer
			if (remaining > m_BufferSize)
			{
				// Write directly to file
				FileStream::Write(remainingData, remaining);
				m_BufferOffset = 0;
			}
			else  
			{
				memcpy(&m_Buffer[0], remainingData, remaining);
				m_BufferOffset = remaining;
			}	
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
		TYR_ASSERT(buffer && m_Operation == Operation::Read && count > 0 && m_MemoryRead > 0);
		const size_t memoryRemaining = m_MemoryRead - m_BufferOffset;
		if (count > memoryRemaining)
		{
			memcpy(buffer, &m_Buffer[m_BufferOffset], memoryRemaining);
			const size_t remaining = count - memoryRemaining;
			uint8* remainingData = &((uint8*)buffer)[memoryRemaining];
			// Handle the case of a size that is bigger than the buffer
			if (remaining > m_BufferSize)
			{
				// Read directly into the buffer passed in
				const size_t memoryRead = FileStream::Read(remainingData, remaining);
				TYR_ASSERT(memoryRead >= remaining);
				// Refill buffer for future read calls
				m_MemoryRead = FileStream::Read(m_Buffer, m_BufferSize);
				m_BufferOffset = 0;
			}
			else
			{
				// Refill buffer to read what remains and for future read calls
				m_MemoryRead = FileStream::Read(m_Buffer, m_BufferSize);
				TYR_ASSERT(m_MemoryRead >= remaining);
				memcpy(remainingData, &m_Buffer[0], remaining);
				m_BufferOffset = remaining;
			}		
		}
		else
		{
			memcpy(buffer, &m_Buffer[m_BufferOffset], count);
			m_BufferOffset += count;
		}
		return count;
	}

	void BufferedFileStream::Skip(size_t count)
	{
		if (m_Operation == Operation::Write)
		{
			if (m_BufferOffset > 0)
			{
				FileStream::Write(m_Buffer, m_BufferOffset);
				m_BufferOffset = 0;
			}
			FileStream::Skip(count);	
		}
		else
		{
			const size_t memoryRemaining = m_MemoryRead - m_BufferOffset;
			if (count >= memoryRemaining)
			{
				FileStream::Skip(count - memoryRemaining);
				m_MemoryRead = FileStream::Read(m_Buffer, m_BufferSize);
				m_BufferOffset = 0;
			}
			else
			{
				m_BufferOffset += count;
			}
		}
		
	}

	BinaryStream::Type BufferedFileStream::GetStreamType() const
	{
		return BinaryStream::Type::File;
	}
}