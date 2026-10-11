#pragma once

#include "Core.h"
#include "IO/BufferedFileStream.h"
#include <type_traits>

namespace tyr
{
	// Bulk reads and writes for plain-data arrays, which skip per-element reflection.
	namespace AssetSerializationUtil
	{
		template<typename T>
		void WriteArray(BufferedFileStream& stream, const Array<T>& array)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Only plain-data arrays can be written in bulk.");
			const uint count = array.Size();
			stream.Write(&count, sizeof(count));
			if (count > 0)
			{
				stream.Write(array.Data(), sizeof(T) * count);
			}
		}

		// Reuses the array's existing capacity.
		template<typename T>
		void ReadArray(BufferedFileStream& stream, Array<T>& array)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Only plain-data arrays can be read in bulk.");
			uint count = 0;
			stream.Read(&count, sizeof(count));
			array.Resize(count);
			if (count > 0)
			{
				stream.Read(array.Data(), sizeof(T) * count);
			}
		}

		inline void WriteString(BufferedFileStream& stream, const char* str)
		{
			const uint length = static_cast<uint>(strlen(str));
			stream.Write(&length, sizeof(length));
			stream.Write(str, length);
		}

		template<uint N>
		void ReadString(BufferedFileStream& stream, LocalString<N>& str)
		{
			uint length = 0;
			stream.Read(&length, sizeof(length));
			char buffer[N + 1];
			const uint kept = length < N ? length : N;
			stream.Read(buffer, kept);
			// Anything past what fits is skipped rather than overflowing.
			for (uint i = kept; i < length; ++i)
			{
				char discard;
				stream.Read(&discard, 1);
			}
			buffer[kept] = '\0';
			str = buffer;
		}
	}
}
