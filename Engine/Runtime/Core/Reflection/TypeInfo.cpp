#include "TypeInfo.h"
#include "String/StringTypes.h"
#include "Identifiers/Identifiers.h"
#include "Serializer.h"

namespace tyr
{
	void TypeInfoUtil::AddField(TypeInfo& info, const String& name, const String& typeName, const CustomObjectSerializer* customSerializer, size_t countOffset, size_t dataOffset, bool isVisible, bool isEditable, bool isCArray)
	{
		Field& field = info.fields[info.fieldCount++];
		field.name = name;
		field.typeID = Id64(typeName.c_str());
		field.customSerializer = customSerializer;
		field.id = Id32(name.c_str());
		field.countOffset = static_cast<uint>(countOffset);
		field.dataOffset = static_cast<uint>(dataOffset);
		field.isVisible = isVisible;
		field.isEditable = isEditable;
		field.isCArray = isCArray;
	}
}