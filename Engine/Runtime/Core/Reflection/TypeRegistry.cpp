#include "TypeRegistry.h"

namespace tyr
{
	TypeRegistry& TypeRegistry::Instance()
	{
		static TypeRegistry registry;
		return registry;
	}

	TypeInfo& TypeRegistry::AddType(const char* name)
	{
		const Id64 id(name);
		TYR_ASSERT(m_TypeMap.find(id) == m_TypeMap.end());
		TypeInfo& typeInfo = m_TypeMap[id];
		typeInfo.name = name;
		return typeInfo;
	}

	const TypeInfo& TypeRegistry::GetType(const Id64& id) const
	{
		TYR_ASSERT(m_TypeMap.find(id) != m_TypeMap.end());
		return m_TypeMap.at(id);
	}

	const TypeInfo& TypeRegistry::GetType(const char* name) const
	{
		return GetType(Id64(name));
	}
}