#include "TypeInfo.h"
#include "String/StringTypes.h"
#include "Identifiers/Identifiers.h"
#include "Serializer.h"

namespace tyr
{
	void TypeInfoUtil::AddField(TypeInfo& info, const char* name, const Id64& typeID, const CustomObjectSerializer* customSerializer, size_t countOffset, size_t dataOffset, bool isVisible, bool isEditable, bool isFinal, bool isCArray)
	{
		// Don't add editor-only / debug fields to the type in final mode as they won't be serialized for final build
#if TYR_FINAL
		if (isFinal)
		{
#endif
			Field& field = info.fields[info.fieldCount++];
			field.name = name;
			field.typeID = typeID;
			field.customSerializer = customSerializer;
			field.id = Id32(name);
			field.countOffset = static_cast<uint>(countOffset);
			field.dataOffset = static_cast<uint>(dataOffset);
			field.isVisible = isVisible;
			field.isEditable = isEditable;
			field.isFinal = isFinal;
			field.isCArray = isCArray;
#if TYR_FINAL
		}
		
#endif
	}
}