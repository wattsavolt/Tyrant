#include "AssetRegistry.h"
#include "Memory/Memory.h"
#include "Reflection/Reflection.h"
#include <cstring>

namespace tyr
{
    TYR_REFL_CLASS_START(SourceAssetData, 0);
        TYR_REFL_FIELD(&SourceAssetData::filePath, "FilePath", true, true);
    TYR_REFL_CLASS_END();

    TYR_REFL_CLASS_START(AssetRegistryFile, 0);
        TYR_REFL_FIELD(&AssetRegistryFile::sourceAssets, "SourceAssets", true, true);
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
}