#pragma once

#include "Containers/HashMap.h"
#include "String/LocalString.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	// Loads and stores config data from a .ini file
	class TYR_CORE_EXPORT Config final
	{
	public:
		using Key = Id64;
		using Value = LocalString<32>;

		static constexpr uint8 c_MaxConfigEntries = 50;

		Config(const char* filePath);

		const Value& GetValue(const char* name) const;

		int GetValueAsInt(const char* name) const;

		bool GetValueAsBool(const char* name) const;

		TYR_FORCEINLINE uint GetValueAsUInt(const char* name) const
		{
			return static_cast<uint>(GetValueAsInt(name));
		}

	private:
		HashMap<Key, Value> m_Map;
	};
}
