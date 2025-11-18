#pragma once

#include "Core.h"
#include "EngineMacros.h"

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

		void Load();
		void Save();
		void AddAsset(AssetID assetID, const char* assetPath, AssetID* refAssetID = nullptr);
		void AddAssetReference(AssetID assetID, AssetID referenceID);
		void UpdateAssetPath(AssetID assetID, const char* assetPath);
		void RemoveAsset(AssetID assetID);
		bool RemoveAssetIfExists(const char* assetPath);
		bool HasAssetPath(const char* assetPath, AssetID& assetID) const;
		int GetAssetRefCount(const char* assetPath) const;

		// Should only be called from a single thread
		const AssetData& GetAssetData(AssetID assetID) const;

	private:
		static constexpr const char* c_AssetRegistryPath = "/AssetRegistry/AssetRegistry.bin";

		AssetRegistry();

		friend class Engine;

		AssetRegistryFile m_RegistryFile;
		mutable Mutex m_Mutex;
	};
	
}