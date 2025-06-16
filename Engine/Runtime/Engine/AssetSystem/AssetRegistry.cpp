#include "AssetRegistry.h"
#include "Memory/Memory.h"
#include "Reflection/Reflection.h"
#include <cstring>

namespace tyr
{
    TYR_REFL_CLASS_START(AssetData, 0);
        TYR_REFL_FIELD(&AssetData::filePath, "FilePath", true, true, true);
        TYR_REFL_FIELD(&AssetData::references, "References", true, true, false);
    TYR_REFL_CLASS_END();

    TYR_REFL_CLASS_START(AssetRegistryFile, 0);
        TYR_REFL_FIELD(&AssetRegistryFile::assets, "Assets", true, true, true);
    TYR_REFL_CLASS_END();

    AssetRegistry::AssetRegistry()
        : m_RegistryFile()
    {

    }

    AssetRegistry& AssetRegistry::Instance()
    {
        static AssetRegistry registry;
        return registry;
    }

    void AssetRegistry::AddAsset(AssetID assetID, const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.find(assetID) == m_RegistryFile.assets.end());
        AssetData& data = m_RegistryFile.assets[assetID];
        data.filePath = assetPath;
        data.references.Reserve(5);
    }

    void AssetRegistry::AddAssetReference(AssetID assetID, AssetID referenceID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.find(assetID) != m_RegistryFile.assets.end());
        AssetData& data = m_RegistryFile.assets[assetID];
        data.references.Add(referenceID);
    }

    void AssetRegistry::UpdateAssetPath(AssetID assetID, const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.find(assetID) != m_RegistryFile.assets.end());
        AssetData& data = m_RegistryFile.assets[assetID];
        data.filePath = assetPath;
    }

    void AssetRegistry::RemoveAsset(AssetID assetID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.find(assetID) != m_RegistryFile.assets.end());
        m_RegistryFile.assets.erase(assetID);
    }

    bool AssetRegistry::HasAssetPath(const char* assetPath, AssetID& assetID)
    {
        LockGuard guard(m_Mutex);
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                assetID = keyVal.first;
                return true;
            }
        }
        return false;
    }

    const AssetData& AssetRegistry::GetAssetData(AssetID assetID)
    {
        TYR_ASSERT(m_RegistryFile.assets.find(assetID) != m_RegistryFile.assets.end());
        return m_RegistryFile.assets[assetID];
    }
}