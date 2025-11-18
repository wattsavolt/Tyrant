#pragma once
#include "EngineMacros.h"
#include "Core.h"

namespace tyr
{
	class TYR_ENGINE_EXPORT AssetUtil final
	{
	public:
		static AssetID CreateAssetID();
		// Note: absFilePath must be an empty preallocated char array that does not contain the null character.
		static void CreateFullPath(char absFilePath[], const char* relativePath, size_t absFilePathSize = TYR_MAX_PATH_TOTAL_SIZE);
	};
	
}