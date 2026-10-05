#pragma once

#include "Containers/HashMap.h"
#include "Containers/Array.h"
#include "String/LocalString.h"
#include "String/Path.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	// Loads, modifies and saves .ini-style "name=value" lines (one per line, no sections or
	// comments) from/to a file. Not thread-safe - callers must serialize their own access.
	class TYR_CORE_API Config final
	{
	public:
		using Key = Id64;
		using Value = LocalString<64>;

		static constexpr uint8 c_MaxEntries = 50;

		// Loads filePath's existing entries if the file is already there - if not, starts
		// empty (the normal first-run case), ready to be populated via SetValue and written
		// out later via Save().
		Config(const char* filePath);

		bool HasValue(const char* name) const;

		const Value& GetValue(const char* name) const;
		int GetValueAsInt(const char* name) const;
		bool GetValueAsBool(const char* name) const;
		TYR_FORCEINLINE uint GetValueAsUInt(const char* name) const
		{
			return static_cast<uint>(GetValueAsInt(name));
		}

		void SetValue(const char* name, const char* value);
		void SetValueAsInt(const char* name, int value);
		void SetValueAsBool(const char* name, bool value);
		TYR_FORCEINLINE void SetValueAsUInt(const char* name, uint value)
		{
			SetValueAsInt(name, static_cast<int>(value));
		}

		// Writes every entry back out to the file this Config was constructed with.
		void Save() const;

	private:
		void Load();
		void ParseLine(const char* line, size_t length);

		HashMap<Key, Value> m_Map;
		// Id64 is a one-way hash - it can't be turned back into "QualityLevel" to write a
		// "QualityLevel=3" line back out, so the original key text has to be kept somewhere
		// too. One entry per key in m_Map, in the order each key was first seen.
		Array<LocalString<32>> m_Keys;
		Path m_FilePath;
	};
}
