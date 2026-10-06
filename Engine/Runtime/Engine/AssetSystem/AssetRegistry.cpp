#include "AssetRegistry.h"
#include "Memory/Memory.h"
#include "BuildConfig.h"
#include "AssetUtil.h"
#include "AssetConstants.h"

namespace tyr
{
    TYR_REFL_CLASS_START(RegAssetData, 0);
        TYR_REFL_FIELD(&RegAssetData::filePath, "FilePath", true, true, true);
        TYR_REFL_FIELD(&RegAssetData::dependencyOffset, "Dependency Offset", true, true, true);
        TYR_REFL_FIELD(&RegAssetData::dependencyCount, "Dependency Count", true, true, true);
    TYR_REFL_CLASS_END();

    TYR_REFL_CLASS_START(AssetRegistryFile, 0);
        TYR_REFL_FIELD(&AssetRegistryFile::assets, "Assets", true, true, true);
        TYR_REFL_FIELD(&AssetRegistryFile::dependencies, "Dependencies", true, true, true);
    TYR_REFL_CLASS_END();

    AssetRegistry::AssetRegistry()
        : m_RegistryFile()
    {
        m_DependenciesBackup.Reserve(m_RegistryFile.dependencies.Capacity());
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
        const StringView fsPath(absAssetRegistryPath);
        if (std::filesystem::exists(fsPath))
        {
            Serializer::Instance().DeserializeFromFile<AssetRegistryFile>(absAssetRegistryPath, m_RegistryFile);
        }
        ++m_Version;
    }

    void AssetRegistry::Save()
    {
#if TYR_EDITOR
        char absAssetRegistryPath[TYR_MAX_PATH_TOTAL_SIZE];
        AssetUtil::CreateFullPath(absAssetRegistryPath, c_AssetRegistryPath);
        PathUtil::CreateDirectoriesInFilePath(absAssetRegistryPath);
        Serializer::Instance().SerializeToFile<AssetRegistryFile>(absAssetRegistryPath, m_RegistryFile);
#endif
    }

#if TYR_EDITOR
    AssetID AssetRegistry::AddOrUpdateAsset(const char* assetPath, const AssetID* dependencies, uint dependencyCount)
    {
        LockGuard guard(m_Mutex);

        AssetID assetID = GetAssetID(assetPath);
        RegAssetData* data = m_RegistryFile.assets.Find(assetID);
        if (!data)
        {
            assetID = AssetUtil::CreateAssetID();
            data = &m_RegistryFile.assets[assetID];
            data->filePath = assetPath;
            data->dependencyOffset = m_RegistryFile.dependencies.Size();
            data->dependencyCount = 0;
            ++m_Version;
        }

        if (dependencyCount == data->dependencyCount)
        {
            for (uint i = 0; i < dependencyCount; ++i)
            {
                m_RegistryFile.dependencies[data->dependencyOffset + i] = dependencies[i];
            }
            return assetID;
        }

        // A different number of dependencies goes on the end, then the old range is compacted away.
        const bool hadDependencies = data->dependencyCount > 0;
        data->dependencyOffset = m_RegistryFile.dependencies.Size();
        data->dependencyCount = static_cast<uint16>(dependencyCount);
        m_RegistryFile.dependencies.Reserve(data->dependencyOffset + dependencyCount);
        for (uint i = 0; i < dependencyCount; ++i)
        {
            m_RegistryFile.dependencies.Add(dependencies[i]);
        }

        if (hadDependencies)
        {
            RebuildDependencies();
        }
        return assetID;
    }

    void AssetRegistry::UpdateAssetPath(AssetID assetID, const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        RegAssetData& data = m_RegistryFile.assets[assetID];
        data.filePath = assetPath;
        ++m_Version;
    }

    void AssetRegistry::RemoveAsset(AssetID assetID)
    {
        LockGuard guard(m_Mutex);
        TYR_ASSERT(m_RegistryFile.assets.Contains(assetID));
        m_RegistryFile.assets.Erase(assetID);
        RebuildDependencies();
        ++m_Version;
    }

    bool AssetRegistry::RemoveAssetIfExists(const char* assetPath)
    {
        LockGuard guard(m_Mutex);
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                m_RegistryFile.assets.Erase(keyVal.first);
                RebuildDependencies();
                ++m_Version;
                return true;
            }
        }
        return false;
    }

    AssetID AssetRegistry::GetAssetIDSafe(const char* assetPath) const
    {
        LockGuard guard(m_Mutex);
        return GetAssetID(assetPath);
    }

    int AssetRegistry::GetAssetDependencyCount(const char* assetPath) const
    {
        LockGuard guard(m_Mutex);
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                return keyVal.second.dependencyCount;
            }
        }
        return -1;
    }

    int AssetRegistry::GetAssetReferenceCount(const char* assetPath) const
    {
        int result = 0;
        LockGuard guard(m_Mutex);
        AssetID id = GetAssetID(assetPath);
        if (AssetUtil::IsValidAssetID(id))
        {
            for (AssetID depID : m_RegistryFile.dependencies)
            {
                if (depID == id)
                {
                    result++;
                }
            }
            return result;
        }

        return -1;
    }

    const RegAssetData& AssetRegistry::GetAssetData(AssetID assetID) const
    {
        return *m_RegistryFile.assets.Find(assetID);
    }

    AssetID AssetRegistry::GetAssetID(const char* assetPath) const
    {
        for (const auto& keyVal : m_RegistryFile.assets)
        {
            if (keyVal.second.filePath == assetPath)
            {
                return keyVal.first;
            }
        }
        return AssetConstants::c_InvalidAssetID;
    }

    const AssetID* AssetRegistry::GetAssetDependencies(AssetID assetID, uint& count) const
    {
        const RegAssetData& data = *m_RegistryFile.assets.Find(assetID);
        count = data.dependencyCount;
        return &m_RegistryFile.dependencies[data.dependencyOffset];
    }

    void AssetRegistry::RebuildDependencies()
    {
        // Make a backup copy
        m_DependenciesBackup = m_RegistryFile.dependencies;
        m_RegistryFile.dependencies.Clear();
        
        for (std::pair<const AssetID&, RegAssetData&> keyVal : m_RegistryFile.assets)
        {
            RegAssetData& data = keyVal.second;
            const uint newOffset = m_RegistryFile.dependencies.Size();
            for (uint i = 0; i < data.dependencyCount; ++i)
            {
                m_RegistryFile.dependencies.Add(m_DependenciesBackup[data.dependencyOffset + i]);
            }
            data.dependencyOffset = newOffset;
        }
        m_DependenciesBackup.Clear();
    }

#endif
}