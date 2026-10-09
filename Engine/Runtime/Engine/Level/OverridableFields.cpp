#include "OverridableFields.h"
#include "Reflection/TypeRegistry.h"
#include "Reflection/CustomObjectPropertiesReflector.h"
#include <cstring>

namespace tyr
{
	namespace
	{
		// Continues the path's FNV-1a hash with the next member's key.
		Id64 CombineKey(const Id64& parentKey, const Id32& memberKey)
		{
			const uint member = memberKey.GetHash();
			uint64 hash = parentKey.GetHash();
			for (uint i = 0; i < sizeof(member); ++i)
			{
				hash ^= (member >> (i * 8)) & 0xFF;
				hash *= c_FNVPrime64;
			}
			return Id64(hash);
		}

		void AddFields(OverridableFields::FieldList& fields, const TypeInfo& typeInfo, uint baseOffset, const Id64& parentKey)
		{
			for (uint i = 0; i < typeInfo.fieldCount; ++i)
			{
				const Field& field = typeInfo.fields[i];
				if (!field.isEditable)
				{
					continue;
				}

				const Id64 key = CombineKey(parentKey, field.key);
				const uint offset = baseOffset + field.dataOffset;
				const TypeInfo* nested = !field.customPropertiesReflector && !field.isCArray ? TypeRegistry::Instance().FindType(field.typeID) : nullptr;
				if (nested && nested->fieldCount > 0)
				{
					AddFields(fields, *nested, offset, key);
					continue;
				}

				TYR_ASSERT(fields.Size() < OverridableFields::c_MaxFields);
				OverridableField& overridable = fields.ExpandOne();
				overridable.key = key;
				overridable.offset = offset;
				overridable.size = field.size;
				overridable.reflector = field.customPropertiesReflector;
			}
		}
	}

	const OverridableFields::FieldList& OverridableFields::Get(const Id64& typeID)
	{
		// Fixed storage, so lists already handed out never move.
		static LocalArray<Id64, c_MaxTypes> s_TypeIDs;
		static LocalArray<FieldList, c_MaxTypes> s_FieldLists;
		for (uint i = 0; i < s_TypeIDs.Size(); ++i)
		{
			if (s_TypeIDs[i] == typeID)
			{
				return s_FieldLists[i];
			}
		}

		TYR_ASSERT(s_TypeIDs.Size() < c_MaxTypes);
		s_TypeIDs.Add(typeID);
		FieldList& fields = s_FieldLists.ExpandOne();
		fields.Clear();
		if (const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(typeID))
		{
			AddFields(fields, *typeInfo, 0, Id64(c_FNVOffsetBasis64));
		}
		return fields;
	}

	const OverridableField* OverridableFields::Find(const FieldList& fields, const Id64& key)
	{
		for (const OverridableField& field : fields)
		{
			if (field.key == key)
			{
				return &field;
			}
		}
		return nullptr;
	}

	bool OverridableFields::Equal(const OverridableField& field, const uint8* objectA, const uint8* objectB)
	{
		const uint8* a = objectA + field.offset;
		const uint8* b = objectB + field.offset;
		return field.reflector ? field.reflector->Equals(a, b) : memcmp(a, b, field.size) == 0;
	}
}
