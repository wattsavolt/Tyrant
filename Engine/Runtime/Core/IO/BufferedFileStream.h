#pragma once

#include "FileStream.h"
#include "Containers/Containers.h"

namespace tyr
{
	class TYR_CORE_EXPORT BufferedFileStream final : public FileStream
	{
	public:
		BufferedFileStream(uint8* buffer, size_t bufferSize, const char* filePath, Operation op = Operation::Read, bool overwrite = true);
		~BufferedFileStream();

		size_t Write(const void* buffer, size_t count) override;

		size_t Read(void* buffer, size_t count)  override;

		void Skip(size_t count) override;

		Type GetStreamType() const override;
	
	private:
		uint8* m_Buffer;
		size_t m_BufferSize;
		size_t m_BufferOffset;
		// Only used for read operation
		size_t m_MemoryRead;
	};
	
}