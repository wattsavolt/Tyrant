#pragma once

#include "Containers/HashMap.h"
#include "String/LocalString.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	// Parses and stores process command line arguments (e.g. "-editor", "-level Level01").
	// A "-key value" pair stores value under key; a bare "-key" (no following value, or the
	// next token is itself a key) stores an empty value - see HasFlag for querying those.
	class TYR_CORE_API CommandLine final
	{
	public:
		using Key = Id64;
		using Value = LocalString<32>;

		static constexpr uint8 c_MaxEntries = 50;

		// Constructs the single instance - call once, from main/WinMain, before Instance() is
		// used anywhere else.
		static void Create(int argc, const char* const* argv);

		static CommandLine& Instance();

		// True if "name" was passed at all (with or without a value) - the safe way to check
		// an optional switch like "-editor", unlike GetValue/GetValueAsInt/GetValueAsBool
		// below, which assert if "name" wasn't passed.
		bool HasFlag(const char* name) const;

		const Value& GetValue(const char* name) const;

		int GetValueAsInt(const char* name) const;

		bool GetValueAsBool(const char* name) const;

		TYR_FORCEINLINE uint GetValueAsUInt(const char* name) const
		{
			return static_cast<uint>(GetValueAsInt(name));
		}

	private:
		CommandLine(int argc, const char* const* argv);

		HashMap<Key, Value> m_Map;
	};
}
