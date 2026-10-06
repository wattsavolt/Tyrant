#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "AssetID.h"

namespace tyr
{
	struct RegAssetData
	{
		AssetPath filePath;
		uint dependencyOffset;
		uint16 dependencyCount;
	};

	struct AssetRegistryFile
	{
		AssetRegistryFile()
			: assets(3000)
		{
			dependencies.Reserve(assets.Size() * 3);
		}

		HashMap<AssetID, RegAssetData> assets;
		Array<AssetID> dependencies;
	};

	class TYR_ENGINE_API AssetRegistry final : public INonCopyable
	{
	public:
		static AssetRegistry& Instance();

		void Load();
		void Save();

#if TYR_EDITOR
		// Registers the asset at the path and sets its dependencies. An asset already at the path
		// keeps its ID, so anything referencing it stays valid after a reimport.
		AssetID AddOrUpdateAsset(const char* assetPath, const AssetID* dependencies = nullptr, uint dependencyCount = 0);
		void UpdateAssetPath(AssetID assetID, const char* assetPath);
		void RemoveAsset(AssetID assetID);
		bool RemoveAssetIfExists(const char* assetPath);
		// Locks, for calling from a thread other than the main thread.
		AssetID GetAssetIDSafe(const char* assetPath) const;
		// Returns the number of assets this asset depends on
		int GetAssetDependencyCount(const char* assetPath) const;
		// Returns the number of assets that depend on this asset
		int GetAssetReferenceCount(const char* assetPath) const;
		const HashMap<AssetID, RegAssetData>& GetAssets() const { return m_RegistryFile.assets; }
		// Changes whenever an asset is added, removed or moved.
		uint GetVersion() const { return m_Version; }
#endif
		// Following functions are not thread safe and should only be called from the main thread
		const RegAssetData& GetAssetData(AssetID assetID) const;
		AssetID GetAssetID(const char* assetPath) const;
		const AssetID* GetAssetDependencies(AssetID assetID, uint& count) const;

	private:
		static constexpr const char* c_AssetRegistryPath = "/AssetRegistry/AssetRegistry.bin";

		AssetRegistry();

#if TYR_EDITOR
		void RebuildDependencies();
#endif

		Array<AssetID> m_DependenciesBackup;
		AssetRegistryFile m_RegistryFile;
		mutable Mutex m_Mutex;
		uint m_Version = 0;
	};
	
}