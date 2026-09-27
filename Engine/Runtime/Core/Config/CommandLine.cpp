#include "CommandLine.h"
#include "Logging/Logger.h"
#include "Platform/Platform.h"
#include <charconv>
#include <cstring>

namespace tyr
{
	namespace
	{
		CommandLine* s_Instance = nullptr;
	}

	void CommandLine::Create(int argc, const char* const* argv)
	{
		TYR_ASSERT(!s_Instance);
		s_Instance = new CommandLine(argc, argv);
	}

	CommandLine& CommandLine::Instance()
	{
		TYR_ASSERT(s_Instance);
		return *s_Instance;
	}

	CommandLine::CommandLine(int argc, const char* const* argv)
		: m_Map(c_MaxEntries)
	{
		for (int i = 0; i < argc; ++i)
		{
			const char* token = argv[i];
			if (token[0] != '-')
			{
				continue;
			}

			const char* nameStart = token;
			while (*nameStart == '-')
			{
				++nameStart;
			}
			const uint nameLen = static_cast<uint>(std::strlen(nameStart));
			if (nameLen == 0)
			{
				continue;
			}

			const Key key(nameStart, nameLen);
			const bool hasValue = (i + 1 < argc) && (argv[i + 1][0] != '-');
			if (hasValue)
			{
				m_Map[key] = argv[i + 1];
				++i;
			}
			else
			{
				m_Map[key] = "";
			}
		}
	}

	bool CommandLine::HasFlag(const char* name) const
	{
		return m_Map.Find(Key(name)) != nullptr;
	}

	const CommandLine::Value& CommandLine::GetValue(const char* name) const
	{
		const Key key = Key(name);
		return *m_Map.Find(key);
	}

	int CommandLine::GetValueAsInt(const char* name) const
	{
		int result;
		const Key key = Key(name);
		const Value& value = *m_Map.Find(key);
		const char* str = value.CStr();
		auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);
		TYR_ASSERT(ec == std::errc());
		return result;
	}

	bool CommandLine::GetValueAsBool(const char* name) const
	{
		const int value = GetValueAsInt(name);
		TYR_ASSERT(value == 0 || value == 1);
		return value != 0;
	}
}
