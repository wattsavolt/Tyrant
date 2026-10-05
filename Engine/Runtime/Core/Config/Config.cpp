#include "Config.h"
#include "IO/FileStream.h"
#include "Memory/StackAllocation.h"
#include "Base/Base.h"
#include <charconv>
#include <cstring>

namespace tyr
{
	Config::Config(const char* filePath)
		: m_Map(c_MaxEntries)
		, m_FilePath(filePath)
	{
		m_Keys.Reserve(c_MaxEntries);
		Load();
	}

	void Config::Load()
	{
		const char* filePath = m_FilePath.CStr();
		if (!fs::exists(filePath))
		{
			// Normal first-run case - nothing recorded yet.
			return;
		}

		const size_t fileSize = FileStream::GetFileSize(filePath);
		if (fileSize == 0)
		{
			return;
		}

		SmartStack<char> bufferStack = SmartStackAlloc<char>((uint)fileSize + 1);
		char* buffer = bufferStack;
		FileStream::ReadAllFile(filePath, buffer);
		buffer[fileSize] = '\0';

		size_t lineStart = 0;
		for (size_t i = 0; i <= fileSize; ++i)
		{
			if (i == fileSize || buffer[i] == '\n')
			{
				size_t lineEnd = i;
				// Trim a trailing \r, in case the file has Windows line endings.
				if (lineEnd > lineStart && buffer[lineEnd - 1] == '\r')
				{
					--lineEnd;
				}

				if (lineEnd > lineStart)
				{
					ParseLine(buffer + lineStart, lineEnd - lineStart);
				}

				lineStart = i + 1;
			}
		}
	}

	void Config::ParseLine(const char* line, size_t length)
	{
		const char* equals = static_cast<const char*>(memchr(line, '=', length));
		if (!equals)
		{
			// Not a "name=value" line - skip it rather than treating it as an error, so a
			// blank or malformed line doesn't stop the rest of the file from loading.
			return;
		}

		const size_t nameLength = equals - line;
		const size_t valueLength = length - nameLength - 1;
		if (nameLength == 0)
		{
			return;
		}

		SetValue(LocalString<32>(line, nameLength).CStr(), LocalString<64>(equals + 1, valueLength).CStr());
	}

	bool Config::HasValue(const char* name) const
	{
		return m_Map.Find(Key(name)) != nullptr;
	}

	const Config::Value& Config::GetValue(const char* name) const
	{
		const Key key = Key(name);
		return *m_Map.Find(key);
	}

	int Config::GetValueAsInt(const char* name) const
	{
		int result;
		const Key key = Key(name);
		const Value& value = *m_Map.Find(key);
		const char* str = value.CStr();
		auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);
		TYR_ASSERT(ec == std::errc());
		return result;
	}

	bool Config::GetValueAsBool(const char* name) const
	{
		const int value = GetValueAsInt(name);
		TYR_ASSERT(value == 0 || value == 1);
		return value != 0;
	}

	void Config::SetValue(const char* name, const char* value)
	{
		const Key key = Key(name);
		Value* existing = m_Map.Find(key);
		if (existing)
		{
			*existing = value;
			return;
		}

		TYR_ASSERT(m_Keys.Size() < c_MaxEntries);
		m_Map.Insert(key, Value(value));
		m_Keys.Add(LocalString<32>(name));
	}

	void Config::SetValueAsInt(const char* name, int value)
	{
		char buffer[16];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		TYR_ASSERT(result.ec == std::errc());
		*result.ptr = '\0';
		SetValue(name, buffer);
	}

	void Config::SetValueAsBool(const char* name, bool value)
	{
		SetValueAsInt(name, value ? 1 : 0);
	}

	void Config::Save() const
	{
		// One line per entry ("name=value\n") - c_MaxEntries(50) * a generous 96 bytes per
		// line comfortably covers every key (<=32 chars) and value (<=64 chars) plus "=\n",
		// so this stays on the stack rather than needing a heap-backed buffer.
		constexpr size_t c_MaxLineLength = 96;
		SmartStack<char> bufferStack = SmartStackAlloc<char>((uint)(c_MaxEntries * c_MaxLineLength));
		char* buffer = bufferStack;

		size_t offset = 0;
		for (const LocalString<32>& name : m_Keys)
		{
			const Value* value = m_Map.Find(Key(name.CStr()));
			TYR_ASSERT(value != nullptr);

			const size_t nameLen = name.Size();
			memcpy(buffer + offset, name.CStr(), nameLen);
			offset += nameLen;
			buffer[offset++] = '=';

			const size_t valueLen = value->Size();
			memcpy(buffer + offset, value->CStr(), valueLen);
			offset += valueLen;
			buffer[offset++] = '\n';
		}

		FileStream::WriteFile(m_FilePath.CStr(), buffer, offset);
	}
}
