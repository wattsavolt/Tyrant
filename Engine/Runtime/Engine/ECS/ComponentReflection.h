#pragma once

#include "EngineMacros.h"
#include "Reflection/Reflection.h"
#include "ComponentRegistry.h"
#include <type_traits>

namespace tyr
{
	#define TYR_COMPONENT_START(type, typeVersion) \
		TYR_REFL_CLASS_START(type, typeVersion) \
			static_assert(std::is_trivially_copyable_v<type>, #type " is a component and must be trivially copyable - Archetype/EcsColumn moves components between archetypes with memcpy, never placement-construct/destruct. Use LocalArray<T,N> instead of Array<T>/HashMap for array-like fields."); \
			ComponentRegistry::Instance().AddComponent(typeID);

	#define TYR_COMPONENT_END() TYR_REFL_CLASS_END()

	#define TYR_COMPONENT_FIELD(fieldPtr, name, isVisible, isEditable, isFinal) TYR_REFL_FIELD(fieldPtr, name, isVisible, isEditable, isFinal)

	#define TYR_COMPONENT_ARRAY_FIELD(countFieldPtr, dataFieldPtr, name, isVisible, isEditable, isFinal) TYR_REFL_ARRAY_FIELD(countFieldPtr, dataFieldPtr, name, isVisible, isEditable, isFinal)
}
