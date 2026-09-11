#include "MaterialAsset.h"

namespace tyr
{
	TYR_REFL_CLASS_START(MaterialAssetFile, 0);
		TYR_REFL_FIELD(&MaterialAssetFile::textures, "Textures", true, true, true);
		TYR_REFL_FIELD(&MaterialAssetFile::type, "Type", true, true, true);
	TYR_REFL_CLASS_END();
}