#pragma once

#include "Core.h"

namespace tyr
{
	// Writes JSON text into a buffer kept between uses, putting in the commas between values itself.
	class JsonWriter final
	{
	public:
		JsonWriter();

		void Clear();

		void BeginObject();
		void EndObject();
		void BeginArray();
		void EndArray();
		// Starts an object member, whose value is written next.
		void Key(const char* key);

		void String(const char* value);
		void String(const char* value, size_t length);
		void Number(double value);
		void Int(int64 value);
		void UInt(uint64 value);
		void Bool(bool value);
		void Null();

		// JSON text that's already valid, such as another writer's output, written as one value.
		void Raw(const char* json);
		void Raw(const char* json, size_t length);

		// A string whose characters are appended as they are, for text that needs no escaping such
		// as base64. Ended with EndRawString.
		void BeginRawString();
		void AppendRaw(const char* text, size_t length);
		void AppendBase64(const uint8* data, size_t size);
		void EndRawString();

		// Shorthands for a member and its value.
		void KeyString(const char* key, const char* value) { Key(key); String(value); }
		void KeyNumber(const char* key, double value) { Key(key); Number(value); }
		void KeyInt(const char* key, int64 value) { Key(key); Int(value); }
		void KeyUInt(const char* key, uint64 value) { Key(key); UInt(value); }
		void KeyBool(const char* key, bool value) { Key(key); Bool(value); }

		const char* Data() const { return m_Buffer.Data(); }
		size_t Size() const { return m_Buffer.Size(); }

	private:
		static constexpr uint c_MaxDepth = 32;

		void BeforeValue();
		void Append(const char* text, size_t length);
		void Append(char c);

		Array<char> m_Buffer;
		// Whether the object or array open at each depth has a value in it yet.
		LocalArray<bool, c_MaxDepth> m_HasValue;
		bool m_AfterKey = false;
	};

	namespace Base64
	{
		constexpr size_t GetEncodedSize(size_t size) { return (size + 2) / 3 * 4; }
		// out must hold GetEncodedSize(size) characters.
		void Encode(const uint8* data, size_t size, char* out);
	}
}
