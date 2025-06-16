#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "AssetPath.h"

namespace tyr
{
	struct AssetData
	{
		AssetPath filePath;
		Array<AssetID> references;
	};

	struct AssetRegistryFile
	{
		HashMap<AssetID, AssetData> assets;
	};

	class TYR_ENGINE_EXPORT AssetRegistry final : public INonCopyable
	{
	public:
		static AssetRegistry& Instance();

		void AddAsset(AssetID assetID, const char* assetPath);
		void AddAssetReference(AssetID assetID, AssetID referenceID);
		void UpdateAssetPath(AssetID assetID, const char* assetPath);
		void RemoveAsset(AssetID assetID);
		bool HasAssetPath(const char* assetPath, AssetID& assetID);
		// Should only be called from a single thread
		const AssetData& GetAssetData(AssetID assetID);

	private:
		AssetRegistry();

		AssetRegistryFile m_RegistryFile;
		Mutex m_Mutex;
	};
	
}