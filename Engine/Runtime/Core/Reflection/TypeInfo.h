#pragma once

#include "Base/Base.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	class CustomObjectSerializer;
	struct Field
	{
		String name;
		Id64 typeID;
		const CustomObjectSerializer* customSerializer = nullptr;
		Id32 id;
		// Count offset only used when the field is a C-style array
		uint countOffset;
		uint dataOffset;
		bool isVisible;
		bool isEditable;
		bool isFinal;
		bool isCArray;
	};

	struct TypeInfo
	{
		static constexpr uint c_MaxFields = 20;
		static constexpr int c_InvalidVersion = -1;

		String name;
		size_t size;
		size_t alignment;
		int version;
		uint fieldCount;
		Field fields[c_MaxFields];
	};

	class TYR_CORE_EXPORT TypeInfoUtil
	{
	public:
		static void AddField(TypeInfo& info, const String& name, const String& typeName, const CustomObjectSerializer* customSerializer, size_t countOffset, size_t dataOffset, bool isVisible, bool isEditable, bool isFinal, bool isCArray);
	};

}