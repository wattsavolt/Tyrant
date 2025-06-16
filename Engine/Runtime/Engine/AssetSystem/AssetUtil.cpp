#include "AssetUtil.h"

namespace tyr
{
	AssetID AssetUtil::CreateAssetID()
	{
		Guid guid;
		Platform::CreateGuid(guid);
		return AssetID(guid);
	}
}