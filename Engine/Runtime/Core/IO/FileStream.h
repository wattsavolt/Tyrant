#pragma once

#include "BinaryStream.h"
#include "Containers/Containers.h"

namespace tyr
{
	class TYR_CORE_EXPORT FileStream : public BinaryStream
	{
	public:
		FileStream(const char* filePath, Operation op = Operation::Read, bool overwrite = true);
		virtual ~FileStream();

		size_t Write(const void* buffer, size_t count) override;

		size_t Read(void* buffer, size_t count) override;

		Type GetStreamType() const override;

		void Skip(size_t count) override;

		void Seek(size_t pos) override;

		size_t GetOffset() const override;

		bool IsEOF() const override;

		void Close() override;

		static String ReadFile(const char* filePath)
		{
			FileStream stream(filePath);
			return stream.ReadAsString();
		}

		static WString ReadFileWide(const char* filePath)
		{
			FileStream stream(filePath);
			return stream.ReadAsWString();
		}
		
		// Expects buffer to be preallocated
		static size_t ReadAllFile(const char* filePath, void* buffer)
		{
			FileStream stream(filePath);
			return stream.Read(buffer, stream.GeSize());
		}

		static size_t ReadAllFile(const char* filePath, Array<uint8>& buffer)
		{
			FileStream stream(filePath);
			const size_t size = stream.GeSize();
			if ((uint)size > buffer.Size())
			{
				buffer.Resize((uint)size);
			}
			return stream.Read(reinterpret_cast<void*>(buffer.Data()), size);
		}

		static void WriteFile(const char* filePath, const void* buffer, size_t bufferSize)
		{
			FileStream stream(filePath, Operation::Write);
			stream.Write(buffer, bufferSize);
		}
	
	private:
		FileHandle m_Handle;
	};
	
}