#pragma once

#include "Core.h"
#include "EngineMacros.h"

namespace tyr
{
	// A small ID we hand out to each component type as it gets reflected, starting at
	// 0 and counting up. It only exists at runtime and is never saved to disk - its
	// only job is to say which bit a component owns in a ComponentMask. If you need
	// an ID that stays the same across saves and reloads, that's still Id64
	// (GetTypeID<T>()).
	using ComponentTypeID = uint;

	// The most distinct component types we'll ever support. Keeping this fixed is
	// what lets an ArchetypeKey be a small, fixed-size bitset instead of a growable
	// array.
	constexpr uint c_MaxComponentTypes = 128;

	class TYR_ENGINE_API ComponentRegistry final
	{
	public:
		static ComponentRegistry& Instance();

		// Call this once, when component reflection registers a new type.
		ComponentTypeID AddComponent(const Id64& typeID);

		// Looks up the ID we already gave a component type.
		ComponentTypeID GetComponentTypeID(const Id64& typeID) const;

		template<typename T>
		static ComponentTypeID GetComponentTypeID()
		{
			static const ComponentTypeID id = Instance().GetComponentTypeID(GetTypeID<T>());
			return id;
		}

	private:
		ComponentRegistry() = default;
		~ComponentRegistry() = default;

		HashMap<Id64, ComponentTypeID> m_ComponentTypeIDs;
		uint m_NextID = 0;
	};

}
