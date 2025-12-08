#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Reflection/Serializer.h"

namespace tyr
{
	class TYR_ENGINE_EXPORT AssetUtil final
	{
	public:
		static AssetID CreateAssetID();
		// Note: absFilePath must be an empty preallocated char array that does not contain the null character.
		static void CreateFullPath(char absFilePath[], const char* relativePath, size_t absFilePathSize = TYR_MAX_PATH_TOTAL_SIZE);

		// file path should be relative to the assets folder in following functions

		template <typename T>
		static void SaveAsset(const char* filePath, const T& asset)
		{
			char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
			CreateFullPath(absFilePath, filePath);
			Serializer::Instance().SerializeToFile<T>(absFilePath, asset);
		}

		template <typename T>
		static void LoadAsset(const char* filePath, T& asset)
		{
			char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
			CreateFullPath(absFilePath, filePath);
			Serializer::Instance().DeserializeFromFile<T>(absFilePath, asset);
		}
	};
	
}