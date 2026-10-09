#pragma once

#include "Base/Base.h"
#include "Identifiers/Identifiers.h"
#include "TypeName.h"

namespace tyr
{
	class CustomObjectSerializer;
	class CustomObjectPropertiesReflector;

	// A field's key from its member pointer's text, e.g. "&Light::range" gives the hash of "range".
	// Saved data finds fields by key, so display names can change freely.
	constexpr Id32 MakeFieldKey(const char* memberPointerText)
	{
		uint start = 0;
		uint length = 0;
		for (; memberPointerText[length] != '\0'; ++length)
		{
			if (memberPointerText[length] == ':')
			{
				start = length + 1;
			}
		}
		return Id32(memberPointerText + start, length - start);
	}

	struct Field
	{
		const char* name;
		Id64 typeID;
		const CustomObjectSerializer* customSerializer = nullptr;
		const CustomObjectPropertiesReflector* customPropertiesReflector = nullptr;
		Id32 key;
		// Count offset only used when the field is a C-style array
		uint countOffset;
		uint dataOffset;
		uint size;
		bool isVisible;
		bool isEditable;
		bool isFinal;
		bool isCArray;
	};

	struct TypeInfo
	{
		static constexpr uint c_MaxFields = 20;
		static constexpr int c_InvalidVersion = -1;

		TypeName name;
		size_t size;
		size_t alignment;
		int version;
		uint fieldCount;
		Field fields[c_MaxFields];
	};

	class TYR_CORE_API TypeInfoUtil
	{
	public:
		static void AddField(TypeInfo& info, const char* name, const Id32& key, const Id64& typeID, const CustomObjectSerializer* customSerializer, const CustomObjectPropertiesReflector* customPropertiesReflector, size_t countOffset, size_t dataOffset, size_t size, bool isVisible, bool isEditable, bool isFinal, bool isCArray);

		// Null when the type has no field with this key.
		static const Field* FindField(const TypeInfo& info, const Id32& key);
	};

}