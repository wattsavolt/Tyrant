#pragma once

#include "Base/Base.h"
#include "Containers/Containers.h"
#include "Identifiers/Identifiers.h"
#include "TypeInfo.h"

namespace tyr
{
	class TYR_CORE_EXPORT TypeRegistry final
	{
	public:
		static TypeRegistry& Instance();

		TypeInfo& AddType(const char* name);
		const TypeInfo& GetType(const Id64& id) const;
		const TypeInfo& GetType(const char* name) const;

	private:
		TypeRegistry() = default;
		~TypeRegistry() = default;

		HashMap<Id64, TypeInfo> m_TypeMap;
	};
}