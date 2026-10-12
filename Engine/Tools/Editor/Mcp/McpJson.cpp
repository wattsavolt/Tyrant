#include "McpJson.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace tyr
{
	JsonWriter::JsonWriter()
	{
		m_Buffer.Reserve(4096);
	}

	void JsonWriter::Clear()
	{
		m_Buffer.Clear();
		m_HasValue.Clear();
		m_AfterKey = false;
	}

	void JsonWriter::Append(const char* text, size_t length)
	{
		const uint start = m_Buffer.Size();
		m_Buffer.Resize(start + (uint)length);
		memcpy(m_Buffer.Data() + start, text, length);
	}

	void JsonWriter::Append(char c)
	{
		m_Buffer.Add(c);
	}

	void JsonWriter::BeforeValue()
	{
		if (m_AfterKey)
		{
			m_AfterKey = false;
			return;
		}
		if (!m_HasValue.IsEmpty())
		{
			if (m_HasValue.Back())
			{
				Append(',');
			}
			m_HasValue.Back() = true;
		}
	}

	void JsonWriter::BeginObject()
	{
		BeforeValue();
		Append('{');
		m_HasValue.Add(false);
	}

	void JsonWriter::EndObject()
	{
		m_HasValue.PopBack();
		Append('}');
	}

	void JsonWriter::BeginArray()
	{
		BeforeValue();
		Append('[');
		m_HasValue.Add(false);
	}

	void JsonWriter::EndArray()
	{
		m_HasValue.PopBack();
		Append(']');
	}

	void JsonWriter::Key(const char* key)
	{
		String(key);
		Append(':');
		m_AfterKey = true;
	}

	void JsonWriter::String(const char* value)
	{
		String(value, strlen(value));
	}

	void JsonWriter::String(const char* value, size_t length)
	{
		BeforeValue();
		Append('"');
		for (size_t i = 0; i < length; ++i)
		{
			const char c = value[i];
			switch (c)
			{
			case '"': Append("\\\"", 2); break;
			case '\\': Append("\\\\", 2); break;
			case '\n': Append("\\n", 2); break;
			case '\r': Append("\\r", 2); break;
			case '\t': Append("\\t", 2); break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					char escaped[8];
					snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<uint>(static_cast<unsigned char>(c)));
					Append(escaped, 6);
				}
				else
				{
					Append(c);
				}
				break;
			}
		}
		Append('"');
	}

	void JsonWriter::Number(double value)
	{
		// JSON has no way to write infinity or NaN.
		if (!std::isfinite(value))
		{
			Null();
			return;
		}
		BeforeValue();
		char text[32];
		const int length = snprintf(text, sizeof(text), "%.9g", value);
		Append(text, (size_t)length);
	}

	void JsonWriter::Int(int64 value)
	{
		BeforeValue();
		char text[24];
		const int length = snprintf(text, sizeof(text), "%lld", static_cast<long long>(value));
		Append(text, (size_t)length);
	}

	void JsonWriter::UInt(uint64 value)
	{
		BeforeValue();
		char text[24];
		const int length = snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(value));
		Append(text, (size_t)length);
	}

	void JsonWriter::Bool(bool value)
	{
		BeforeValue();
		Append(value ? "true" : "false", value ? 4 : 5);
	}

	void JsonWriter::Null()
	{
		BeforeValue();
		Append("null", 4);
	}

	void JsonWriter::Raw(const char* json)
	{
		Raw(json, strlen(json));
	}

	void JsonWriter::Raw(const char* json, size_t length)
	{
		BeforeValue();
		Append(json, length);
	}

	void JsonWriter::BeginRawString()
	{
		BeforeValue();
		Append('"');
	}

	void JsonWriter::AppendRaw(const char* text, size_t length)
	{
		Append(text, length);
	}

	void JsonWriter::AppendBase64(const uint8* data, size_t size)
	{
		const uint start = m_Buffer.Size();
		m_Buffer.Resize(start + (uint)Base64::GetEncodedSize(size));
		Base64::Encode(data, size, m_Buffer.Data() + start);
	}

	void JsonWriter::EndRawString()
	{
		Append('"');
	}

	namespace Base64
	{
		void Encode(const uint8* data, size_t size, char* out)
		{
			static const char c_Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			size_t i = 0;
			for (; i + 2 < size; i += 3)
			{
				const uint triple = (uint)data[i] << 16 | (uint)data[i + 1] << 8 | data[i + 2];
				*out++ = c_Alphabet[(triple >> 18) & 63];
				*out++ = c_Alphabet[(triple >> 12) & 63];
				*out++ = c_Alphabet[(triple >> 6) & 63];
				*out++ = c_Alphabet[triple & 63];
			}

			// The last one or two bytes are padded out with '='.
			const size_t remaining = size - i;
			if (remaining > 0)
			{
				const uint triple = (uint)data[i] << 16 | (remaining > 1 ? (uint)data[i + 1] << 8 : 0u);
				*out++ = c_Alphabet[(triple >> 18) & 63];
				*out++ = c_Alphabet[(triple >> 12) & 63];
				*out++ = remaining > 1 ? c_Alphabet[(triple >> 6) & 63] : '=';
				*out++ = '=';
			}
		}
	}
}
