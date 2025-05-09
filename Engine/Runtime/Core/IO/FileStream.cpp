#include "FileStream.h"
#include "Platform/Platform.h"

namespace tyr
{
	FileStream::FileStream(const char* filePath, Operation op, bool overwrite)
		: BinaryStream(op)
	{
		FileAccess access;
		FileCreationMode creationMode;
		if (m_Operation == Operation::Read)
		{
			access = FileAccess::Read;
			creationMode = FileCreationMode::OpenExisting;
		}
		else
		{
			access = FileAccess::Write;
			creationMode = overwrite ? FileCreationMode::CreateAlways : FileCreationMode::OpenAlways;
		}
		m_Name = filePath;
		m_Handle = Platform::OpenOrCreateFile(filePath, access, creationMode);
		m_Size = Platform::GetSizeOfFile(m_Handle);
	}

	FileStream::~FileStream()
	{
		if (m_Handle)
		{
			Close();
		}
	}

	size_t FileStream::Write(const void* buffer, size_t count)
	{
		TYR_ASSERT(buffer && m_Handle && m_Operation == Operation::Write);
		size_t bytesWritten = 0;
		if (m_Handle && m_Operation == Operation::Write)
		{
			Platform::WriteToFile(m_Handle, static_cast<const uint8*>(buffer), count, bytesWritten);
		}
		return bytesWritten;
	}

	size_t FileStream::Read(void* buffer, size_t count) const
	{
		TYR_ASSERT(buffer && m_Handle && m_Operation == Operation::Read);
		size_t bytesRead;
		if (m_Handle && m_Operation == Operation::Read)
		{
			Platform::ReadFromFile(m_Handle, static_cast<uint8*>(buffer), count, bytesRead);
		}
		return bytesRead;
	}

	BinaryStream::Type FileStream::GetStreamType() const
	{
		return BinaryStream::Type::File;
	}

	void FileStream::Skip(size_t count)
	{
		size_t pos = Platform::GetFilePosition(m_Handle);
		Platform::SetFilePosition(m_Handle, pos + count);
	}

	void FileStream::Seek(size_t pos)
	{
		Platform::SetFilePosition(m_Handle, pos);
	}

	size_t FileStream::GetOffset() const
	{
		return Platform::GetFilePosition(m_Handle);
	}

	bool FileStream::IsEOF() const
	{
		return Platform::IsEOF(m_Handle);
	}

	void FileStream::Close()
	{
		Platform::CloseFile(m_Handle);
		m_Name = "";
		m_Handle = nullptr;
		m_Size = 0;
	}
}