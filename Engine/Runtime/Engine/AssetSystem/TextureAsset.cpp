#include "TextureAsset.h"

namespace tyr
{
	TYR_REFL_CLASS_START(TextureMetadata, 0);
		TYR_REFL_FIELD(&TextureMetadata::assetID, "AssetID", true, true, true);
		TYR_REFL_FIELD(&TextureMetadata::info, "Info", true, true, true);
		TYR_REFL_FIELD(&TextureMetadata::dataSize, "Data Size", true, true, true);
	TYR_REFL_CLASS_END();
}