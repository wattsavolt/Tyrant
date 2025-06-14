#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "AssetPath.h"

namespace tyr
{
	struct SourceAssetData
	{
		// Potential TODO: Add references from other assets later
		AssetPath filePath;
	};

	struct AssetRegistryFile
	{
		HashMap<AssetID, SourceAssetData> sourceAssets;
	};

	class TYR_ENGINE_EXPORT AssetRegistry final : public INonCopyable
	{
	public:
		static AssetRegistry& Instance();

		//void AddAsset(const AssetData& asset);
		//void AddAsset(AssetData&& asset);
	private:
		AssetRegistry();

		AssetRegistryFile m_RegistryFile;
		Mutex m_Mutex;
	};
	
}