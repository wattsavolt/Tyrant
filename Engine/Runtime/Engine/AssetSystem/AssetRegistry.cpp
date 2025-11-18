#include "AssetRegistry.h"
#include "Memory/Memory.h"
#include "BuildConfig.h"
#include "AssetUtil.h"

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

    void AssetRegistry::Load()
    {
        char absAssetRegistryPath[TYR_MAX_PATH_TOTAL_SIZE];
        AssetUtil::CreateFullPath(absAssetRegistryPath, c_AssetRegistryPath);
        const fs::path fsPath = absAssetRegistryPath;
        if (std::filesystem::exists(fsPath))
        {
            Serializer::Instance().DeserializeFromFile<AssetRegistryFile>(absAssetRegistryPath, m_RegistryFile);
        }
    }

    void AssetRegistry::Save()
    {
        char absAssetRegistryPath[TYR_MAX_PATH_TOTAL_SIZE];
        AssetUtil::CreateFullPath(absAssetRegistryPath, c_AssetRegistryPath);
        PathUtil::CreateDirectoriesInFilePath(absAssetRegistryPath);
        Serializer::Instance().SerializeToFile<AssetRegistryFile>(absAssetRegistryPath, m_RegistryFile);
    }

    void AssetRegistry::AddAsset(AssetID assetID, const char* assetPath, AssetID* refAssetID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(!m_RegistryFile.assets.Contains(assetID));
        AssetData& data = m_RegistryFile.assets[assetID];
        data.filePath = assetPath;
        data.references.Clear();
        data.references.Reserve(5);

        if (refAssetID)
        {
            data.references.Add(*refAssetID);
        }
    }

    void AssetRegistry::AddAssetReference(AssetID assetID, AssetID referenceID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        AssetData& data = m_RegistryFile.assets[assetID];
        data.references.Add(referenceID);
    }

    void AssetRegistry::UpdateAssetPath(AssetID assetID, const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        AssetData& data = m_RegistryFile.assets[assetID];
        data.filePath = assetPath;
    }

    void AssetRegistry::RemoveAsset(AssetID assetID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        m_RegistryFile.assets.Erase(assetID);
    }

    bool AssetRegistry::RemoveAssetIfExists(const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                m_RegistryFile.assets.Erase(keyVal.first);
                return true;
            }
        }
        return false;
    }

    bool AssetRegistry::HasAssetPath(const char* assetPath, AssetID& assetID) const
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

    int AssetRegistry::GetAssetRefCount(const char* assetPath) const
    {
        LockGuard guard(m_Mutex);
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                return keyVal.second.references.Size();
            }
        }
        return -1;
    }

    const AssetData& AssetRegistry::GetAssetData(AssetID assetID) const
    {
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        return *m_RegistryFile.assets.Find(assetID);
    }
}