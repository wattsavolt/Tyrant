#include "TextureAsset.h"

namespace tyr
{
	TYR_REFL_CLASS_START(TextureHeader, 0);
		TYR_REFL_FIELD(&TextureHeader::info, "Info", true, true, true);
		TYR_REFL_FIELD(&TextureHeader::dataSize, "Data Size", true, true, true);
	TYR_REFL_CLASS_END();
}