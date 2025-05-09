#include "BinaryStream.h"
#include "String/StringUtil.h"
#include "Memory/StackAllocation.h"

namespace tyr
{
	BinaryStream::BinaryStream(Operation op)
		: m_Name("")
		, m_Size(0)
		, m_Operation(op)
	{
		
	}

	BinaryStream::~BinaryStream()
	{
		
	}

	void BinaryStream::WriteString(const String& string, StringEncoding encoding)
	{
		if (encoding == StringEncoding::UTF16)
		{
			// Write BOM
			const uint8 bom[2] = { 0xFF, 0xFE };
			Write(bom, sizeof(bom));

			const U16String u16String = StringUtil::UTF8ToUTF16(string);
			Write(u16String.data(), u16String.length() * sizeof(char16_t));
		}
		else
		{
			// Write BOM
			const uint bom[3] = { 0xEF, 0xBB, 0xBF };
			Write(bom, sizeof(bom));

			Write(string.data(), string.length());
		}
	}

	void BinaryStream::WriteString(const WString& string, StringEncoding encoding)
	{
		if (encoding == StringEncoding::UTF16)
		{
			// Write BOM
			const uint bom[2] = { 0xFF, 0xFE };
			Write(bom, sizeof(bom));

			const U16String u16String = StringUtil::WStringToUTF16(string);
			Write(u16String.data(), u16String.length() * sizeof(char16_t));
		}
		else
		{
			// Write BOM
			const uint bom[3] = { 0xEF, 0xBB, 0xBF };
			Write(bom, sizeof(bom));

			const String u8string = StringUtil::WStringToUTF8(string);
			Write(u8string.data(), u8string.length());
		}
	}

	String BinaryStream::ReadAsString()
	{
		// Ensure read from begin of stream
		Seek(0);

		// Try reading header
		uint8 headerBytes[4];
		const size_t numHeaderBytes = Read(headerBytes, 4);

		size_t dataOffset = 0;
		if (numHeaderBytes >= 4)
		{
			if (StringUtil::IsUTF32LE(headerBytes))
			{
				dataOffset = 4;
			}
			else if (StringUtil::IsUTF32BE(headerBytes))
			{
				// UTF-32 big endian decoding is not supported.
				TYR_ASSERT(false);
				return "";
			}
		}

		if (dataOffset == 0)
		{
			if (numHeaderBytes >= 3)
			{
				if (StringUtil::IsUTF8(headerBytes))
				{
					dataOffset = 3;
				}
			}

			if (numHeaderBytes >= 2)
			{
				if (StringUtil::IsUTF16LE(headerBytes))
				{
					dataOffset = 2;
				}
				else if (StringUtil::IsUTF16BE(headerBytes))
				{
					// UTF-16 big endian decoding is not supported.
					TYR_ASSERT(false);
					return "";
				}
			}
		}

		Seek(dataOffset);

		// Read the entire buffer - ideally in one read, but if the size of the buffer is unknown, do multiple fixed size
		// reads.
		const size_t bufferSize = (m_Size > 0 ? m_Size : 4096);
		auto buffer = StackAlloc<std::stringstream::char_type>((uint)bufferSize);

		StringStream result;
		while (!IsEOF())
		{
			const size_t numReadBytes = Read(buffer, bufferSize);
			result.write(buffer, numReadBytes);
		}

		StackFreeLast();

		String string = result.str();

		switch (dataOffset)
		{
			case 0: // Assume UTF-8 if no BOM
			case 3: // UTF-8
				return string;
			case 2: // UTF-16
			{
				uint numChars = (uint)string.length() / 2;

				return StringUtil::UTF16ToUTF8(U16String((char16_t*)string.data(), numChars));
			}
			case 4: // UTF-32
			{
				uint numChars = (uint)string.length() / 4;

				return StringUtil::UTF32ToUTF8(U32String((char32_t*)string.data(), numChars));
			}
			default:
				// Assume ANSI if other encodings not recognized.
				return StringUtil::ANSIToUTF8(string);
		}
	}

	WString BinaryStream::ReadAsWString()
	{
		String str = ReadAsString();
		return StringUtil::ToWString(str);
	}
}