#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Identifiers/Identifiers.h"

namespace tyr
{
	// A component field holding an asset ID, with the file extension of the assets it takes, so the
	// editor can show an asset picker for it. Games list their own components' fields here too.
	struct AssetField
	{
		Id64 componentType;
		Id32 fieldKey;
		const char* extension;
	};

	namespace AssetFields
	{
		constexpr uint c_MaxAssetFields = 128;

		TYR_ENGINE_API void Add(const Id64& componentType, const Id32& fieldKey, const char* extension);
		TYR_ENGINE_API const AssetField* GetAll(uint& outCount);
	}
}
