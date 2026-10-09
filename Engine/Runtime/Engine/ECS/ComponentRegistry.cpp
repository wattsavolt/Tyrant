#include "ComponentRegistry.h"

namespace tyr
{
	ComponentRegistry& ComponentRegistry::Instance()
	{
		static ComponentRegistry registry;
		return registry;
	}

	ComponentTypeID ComponentRegistry::AddComponent(const Id64& typeID)
	{
		TYR_ASSERT(!m_ComponentTypeIDs.Contains(typeID));

		if (m_NextID >= c_MaxComponentTypes)
		{
			TYR_LOG_FATAL("Exceeded maximum number of reflected component types.");
		}

		const ComponentTypeID compTypeID = m_NextID++;
		m_ComponentTypeIDs.Insert(typeID, compTypeID);
		m_TypeIDs[compTypeID] = typeID;
		return compTypeID;
	}

	ComponentTypeID ComponentRegistry::GetComponentTypeID(const Id64& typeID) const
	{
		TYR_ASSERT(m_ComponentTypeIDs.Contains(typeID));
		return *m_ComponentTypeIDs.Find(typeID);
	}

	bool ComponentRegistry::FindComponentTypeID(const Id64& typeID, ComponentTypeID& outComponentTypeID) const
	{
		const ComponentTypeID* compTypeID = m_ComponentTypeIDs.Find(typeID);
		if (!compTypeID)
		{
			return false;
		}
		outComponentTypeID = *compTypeID;
		return true;
	}

	const Id64& ComponentRegistry::GetReflectionTypeID(ComponentTypeID componentTypeID) const
	{
		TYR_ASSERT(componentTypeID < m_NextID);
		return m_TypeIDs[componentTypeID];
	}
}
