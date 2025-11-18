#pragma once

#include "Base/base.h"
#include "LocalString.h"

namespace tyr
{
	class PathConstants
	{
	public:
		static constexpr uint c_MaxFileName = TYR_MAX_FILENAME;
		// Include null-terminated character
		static constexpr uint c_MaxFileNameTotalSize = c_MaxFileName + 1;
		static constexpr uint c_MaxPath = TYR_MAX_PATH;
		// Include null-terminated character
		static constexpr uint c_MaxPathTotalSize = c_MaxPath + 1;

		static constexpr uint c_MaxRelativePath = 127;
		static constexpr uint c_MaxRelativePathTotalSize = c_MaxRelativePath + 1;

		static constexpr uint c_MaxAssetName = c_MaxFileName;
		static constexpr uint c_MaxAssetNameTotalSize = c_MaxAssetName + 1;
		static constexpr uint c_MaxAssetPath = c_MaxRelativePath;
		static constexpr uint c_MaxAssetPathTotalSize = c_MaxAssetPath + 1;
	};

	using FileName = LocalString<PathConstants::c_MaxFileName>;
	using Path = LocalString<PathConstants::c_MaxPath>;
	using RelativePath = LocalString<PathConstants::c_MaxRelativePath>;
	using AssetPath = LocalString<PathConstants::c_MaxAssetPath>;
}