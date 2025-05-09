#pragma once

#include <string>
#include <sstream>

namespace tyr
{
	enum class StringEncoding
	{
		UTF8,
		UTF16
	};

	/// Basic string that uses general custom memory allocator. 
	template <typename T>
	using BasicString = std::basic_string < T, std::char_traits<T>>;

	/// Wide string used primarily for handling Unicode text (UTF-16 on Windows and UTF-32 on Linux). 
	using WString = std::wstring;

	/// Narrow string used for handling narrow encoded text (either locale specific ANSI or UTF-8). 
	using String = std::string;

	/// Wide string used UTF-16 encoded strings. 
	using U8String = std::u8string;

	/// Wide string used UTF-16 encoded strings. 
	using U16String = std::u16string;

	/// Wide string used UTF-32 encoded strings. 
	using U32String = std::u32string;

	///	Basic string stream that uses engine's default memory allocators. 
	template <typename T>
	using BasicStringStream = std::basic_stringstream <T, std::char_traits<T>>;

	/// String stream for either locale specific ANSI or UTF-8 strings. 
	using StringStream = std::stringstream;

	/// String stream for wide strings. 
	using WStringStream = std::wstringstream;

	/// String stream mainly used for UTF-8 strings. 
	using U8StringStream = BasicStringStream<char8_t>;

	/// String stream mainly used for UTF-16 strings. 
	using U16StringStream = BasicStringStream<char16_t>;

	/// String stream mainly used for UTF-32 strings. 
	using U32StringStream = BasicStringStream<char32_t>;	
}