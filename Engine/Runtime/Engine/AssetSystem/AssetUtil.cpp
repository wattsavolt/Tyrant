#include "AssetUtil.h"
#include "BuildConfig.h"
#include <cstring>

namespace tyr
{
	AssetID AssetUtil::CreateAssetID()
	{
		AssetID assetID;
		while (assetID.GetHash() == 0)
		{
			Guid guid;
			Platform::CreateGuid(guid);
			assetID = guid;
		}
		return assetID;
	}

	void AssetUtil::CreateFullPath(char absFilePath[], const char* relativePath, size_t absFilePathSize)
	{
		static const size_t assetDirSize = strlen(c_AssetsDir);
		const size_t relPathSize = strlen(relativePath);
		// Max usable characters (excluding '\0')
		TYR_ASSERT(assetDirSize + 1 + relPathSize <= TYR_MAX_PATH);
		// Second arg is remaining space in destination buffer
		strcpy_s(absFilePath, absFilePathSize, c_AssetsDir);
		// Add slash
		strcat_s(absFilePath, absFilePathSize, "/");
		// Add relative path
		strcat_s(absFilePath, absFilePathSize, relativePath);
	}
}