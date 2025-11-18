#pragma once

#include "Base/base.h"

namespace tyr
{
	template <uint N>
	class LocalString final
	{
	public:
		// Add 1 for the null character
		static constexpr uint c_Capacity = N + 1;
	private:
		void Copy(const char* data)
		{
			m_TotalSize = strlen(data) + 1;
			TYR_ASSERT(m_TotalSize <= c_Capacity);
			strcpy_s(m_Data, m_TotalSize, data);
		}

		void Copy(const char* data, size_t size)
		{
			m_TotalSize = size + 1;
			TYR_ASSERT(m_TotalSize <= c_Capacity);
			memcpy(m_Data, data, size);
			m_Data[size] = '\0';
		}

		friend class Serializer;
		char m_Data[c_Capacity];
		size_t m_TotalSize;

	public:
		constexpr void Reset()
		{
			m_TotalSize = 1;
			m_Data[0] = '\0';
		}

		constexpr LocalString() noexcept
		{
			Reset();
		}

		LocalString(const char* data)
		{
			Reset();
			Copy(data);
		}

		// Useful when only want a substring of the data
		LocalString(const char* data, size_t size)
		{
			Reset();
			Copy(data, size);
		}

		LocalString(const LocalString& other)
			: LocalString(other.m_Data)
		{

		}

		~LocalString() = default;

		LocalString& operator=(const char* data)
		{
			if (m_Data != data)
			{
				Copy(data);
			}
			return *this;
		}

		LocalString& operator=(const LocalString& other)
		{
			*this = other.m_Data;
			return *this;
		}

		LocalString& operator+=(const LocalString& other)
		{
			strcpy_s(&m_Data[m_TotalSize - 1], c_Capacity, other.m_Data);
			return *this;
		}

		bool operator==(const LocalString& other) const
		{
			// 0 if they are equal
			return strcmp(m_Data, other.m_Data) == 0;
		}

		bool operator==(const char* data) const
		{
			if (data == nullptr)
			{
				return false;
			}
			return strcmp(m_Data, data) == 0;
		}

		friend LocalString operator+ (const LocalString& lhs, const LocalString& rhs)
		{
			const uint size = lhs.m_TotalSize + rhs.m_TotalSize - 1;
			TYR_ASSERT(size <= c_Capacity);
			LocalString str;
			strcpy_s(str.m_Data, str.c_Capacity, lhs.m_Data);
			strcpy_s(&str.m_Data[lhs.m_TotalSize - 1], str.c_Capacity - lhs.c_Capacity, rhs.m_Data);
			str.m_TotalSize = size;
			return str;
		}

		friend LocalString operator+ (const LocalString& lhs, int rhs)
		{
			const uint lhsSize = lhs.m_TotalSize - 1;
			LocalString str;
			strcpy_s(str.m_Data, str.c_Capacity, lhs.m_Data);
			char* first = str.m_Data + lhsSize;
			const auto result = std::to_chars(first, str.m_Data + str.c_Capacity, rhs);
			TYR_ASSERT(result.ec == std::errc());
			*result.ptr = '\0';
			str.m_TotalSize = lhs.m_TotalSize + (result.ptr - first);
			return str;
		}

		// Useful for assigning this string to concatenation of other strings 
		template <typename... Args>
		void Set(const Args*... args) 
		{
			TYR_STATIC_ASSERT((std::is_convertible_v<const Args*, const char*> && ...),
				"All arguments must be const char*");

			// Calculate total size including null
			m_TotalSize = (strlen(args) + ...) + 1;
			TYR_ASSERT(m_TotalSize <= c_Capacity);

			char* out = m_Data;

			// Copy each string into buffer (fold expression over memcpy)
			auto copy = [&out](const char* s) {
				const size_t len = strlen(s);
				std::memcpy(out, s, len);
				out += len;
				};
			(copy(args), ...);

			*out = '\0'; // null-terminate
		}

		constexpr size_t Size() const { return m_TotalSize - 1; }

		constexpr size_t BufferSize() const { return m_TotalSize; }

		constexpr char* Str() { return m_Data; }

		constexpr const char* CStr() const { return m_Data; }
	};
	
}