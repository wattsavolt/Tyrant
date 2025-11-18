#pragma once

#include "Base/Base.h"
#include "TypeRegistry.h"
#include "ReflectionUtil.h"
#include "Serializer.h"

namespace tyr
{
// These two macros need to be defined here as defining elsewhere can lead to spaces being added for some reason.
#define TYR_REFL_CONCAT_INNER(a, b) a##b
#define TYR_REFL_CONCAT(a, b) TYR_REFL_CONCAT_INNER(a, b)

// Only suited to non-template types. For template types, use a custom serialiser 
#define TYR_REFL_CLASS_START(type, typeVersion) \
    struct type##MetaClass \
    { \
        type##MetaClass() \
        { \
            const StringView typeName = GetTypeName<type>(); \
            const Id64 typeID(typeName.data(), typeName.size()); \
            TypeInfo& info = TypeRegistry::Instance().AddType(typeID); \
            info.name = TypeName(typeName.data(), typeName.size()); \
            info.size = sizeof(type); \
            info.alignment = alignof(type); \
            info.version = typeVersion; \
            info.fieldCount = 0; 

#define TYR_REFL_CLASS_END() \
        } \
    } TYR_REFL_CONCAT(metaClassInst_, __LINE__); 

#define TYR_REFL_FIELD(fieldPtr, name, isVisible, isEditable, isFinal) TypeInfoUtil::AddField(info, name, GetFieldTypeID(fieldPtr), GetFieldBuiltInCustomObjectSerializer(fieldPtr), 0, GetFieldOffset(fieldPtr), isVisible, isEditable, isFinal, false);

#define TYR_REFL_ARRAY_FIELD(countFieldPtr, dataFieldPtr, name, isVisible, isEditable, isFinal) TypeInfoUtil::AddField(info, name, GetFieldTypeID(dataFieldPtr), nullptr, GetFieldOffset(countFieldPtr), GetFieldOffset(dataFieldPtr), isVisible, isEditable, isFinal, true);
}


