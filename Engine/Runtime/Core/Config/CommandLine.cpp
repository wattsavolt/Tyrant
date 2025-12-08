#include "CommandLine.h"
#include "Logging/Logger.h"
#include "Platform/Platform.h"
#include <charconv>

namespace tyr
{
	CommandLine::CommandLine(const char* filePath)
		: m_Map(c_MaxEntries)
	{
		
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
