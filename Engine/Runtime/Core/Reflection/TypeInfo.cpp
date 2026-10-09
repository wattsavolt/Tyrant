#include "TypeInfo.h"
#include "String/StringTypes.h"
#include "Identifiers/Identifiers.h"
#include "Serializer.h"

namespace tyr
{
	void TypeInfoUtil::AddField(TypeInfo& info, const char* name, const Id32& key, const Id64& typeID, const CustomObjectSerializer* customSerializer, const CustomObjectPropertiesReflector* customPropertiesReflector, size_t countOffset, size_t dataOffset, size_t size, bool isVisible, bool isEditable, bool isFinal, bool isCArray)
	{
		// Don't add editor-only / debug fields to the type in final mode as they won't be serialized for final build
#if TYR_FINAL
		if (isFinal)
		{
#endif
			TYR_ASSERT(info.fieldCount < TypeInfo::c_MaxFields);
			// Saved data finds fields by key, so two fields can't share one.
			TYR_ASSERT(!FindField(info, key));

			Field& field = info.fields[info.fieldCount++];
			field.name = name;
			field.typeID = typeID;
			field.customSerializer = customSerializer;
			field.customPropertiesReflector = customPropertiesReflector;
			field.key = key;
			field.countOffset = static_cast<uint>(countOffset);
			field.dataOffset = static_cast<uint>(dataOffset);
			field.size = static_cast<uint>(size);
			field.isVisible = isVisible;
			field.isEditable = isEditable;
			field.isFinal = isFinal;
			field.isCArray = isCArray;
#if TYR_FINAL
		}

#endif
	}

	const Field* TypeInfoUtil::FindField(const TypeInfo& info, const Id32& key)
	{
		for (uint i = 0; i < info.fieldCount; ++i)
		{
			if (info.fields[i].key == key)
			{
				return &info.fields[i];
			}
		}
		return nullptr;
	}
}
