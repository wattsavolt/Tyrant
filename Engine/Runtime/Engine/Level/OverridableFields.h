#pragma once

#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	class CustomObjectPropertiesReflector;

	// A value in a reflected type that a placed actor can change from its actor type's default.
	struct OverridableField
	{
		// Made from the member names along the path to it, so it survives fields being reordered.
		Id64 key;
		uint offset;
		uint size;
		// Set for values compared through a reflector, like a LocalArray.
		const CustomObjectPropertiesReflector* reflector;
	};

	class TYR_ENGINE_API OverridableFields final
	{
	public:
		static constexpr uint c_MaxFields = 32;
		static constexpr uint c_MaxTypes = 128;
		using FieldList = LocalArray<OverridableField, c_MaxFields>;

		// Every editable value in the reflected type, nested ones included. Built on first use.
		static const FieldList& Get(const Id64& typeID);

		// Null when the type has no field with this key.
		static const OverridableField* Find(const FieldList& fields, const Id64& key);

		static bool Equal(const OverridableField& field, const uint8* objectA, const uint8* objectB);
	};
}
