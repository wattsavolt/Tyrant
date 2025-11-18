#pragma once

#include "StringTypes.h"

namespace tyr
{
	class TYR_CORE_EXPORT StringUtil 
	{
	public:
		template<typename ... Args>
		static String FormatString(const String& format, Args ... args)
		{
			const int sizeS = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
			if (sizeS <= 0)
			{
				return "";
			}
			const size_t size = static_cast<size_t>(sizeS);
			char* buf = std::make_unique<char[]>(size);
			std::snprintf(buf.get(), size, format.c_str(), args ...);
			return String(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
		}
	
		/// Checks if the UTF-32 buffer has a byte order mark in little endian. 
		static bool IsUTF32LE(const uint8* buffer)
		{
			return buffer[0] == 0xFF && buffer[1] == 0xFE && buffer[2] == 0x00 && buffer[3] == 0x00;
		}

		/// Checks if the UTF-32 buffer has a byte order mark in big endian. 
		static bool IsUTF32BE(const uint8* buffer)
		{
			return buffer[0] == 0x00 && buffer[1] == 0x00 && buffer[2] == 0xFE && buffer[3] == 0xFF;
		}

		/// Checks if the UTF-16 buffer has a byte order mark in little endian.
		static bool IsUTF16LE(const uint8* buffer)
		{
			return buffer[0] == 0xFF && buffer[1] == 0xFE;
		}

		/// Checks if the UTF-16 buffer has a byte order mark in big endian. 
		static bool IsUTF16BE(const uint8* buffer)
		{
			return buffer[0] == 0xFE && buffer[1] == 0xFF;
		}

		/// Checks if the buffer has a UTF-8 byte order mark. 
		static bool IsUTF8(const uint8* buffer)
		{
			return (buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF);
		}

		static WString ToWString(const String& str)
		{
			return WString(str.begin(), str.end());
		}

		static WString ToWString(const char* str)
		{
			return ToWString(String(str));
		}

		// Note: Both src and dst must have been appropriately allocated
		static void ToWString(const char* src, wchar_t* dst)
		{
			TYR_ASSERT(src && dst);

			const size_t len = strlen(src);

			for (size_t i = 0; i < len; ++i)
			{
				// ASCII/UTF-8 safe: directly widen each byte
				dst[i] = static_cast<wchar_t>(static_cast<unsigned char>(src[i]));
			}

			dst[len] = L'\0'; // null-terminate
		}

		static String ToString(const WString& str)
		{
			return String(str.begin(), str.end());
		}

		static String ToString(const wchar_t* str)
		{
			return ToString(WString(str));
		}

		static String WStringToUTF8(const WString& input);

		static U16String WStringToUTF16(const WString& input);

		static U16String UTF8ToUTF16(const String& input);

		static String UTF16ToUTF8(const U16String& input);

		static String UTF32ToUTF8(const U32String& input);

		static String ANSIToUTF8(const String& input);
	};
	
}