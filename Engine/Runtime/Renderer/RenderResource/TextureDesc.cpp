#include "TextureDesc.h"
#include "Core.h"

namespace tyr
{
	TYR_REFL_CLASS_START(PixelFormat, 0);
	TYR_REFL_CLASS_END();

	TYR_REFL_CLASS_START(ImageType, 0);
	TYR_REFL_CLASS_END();

	TYR_REFL_CLASS_START(TextureInfo, 0);
		TYR_REFL_FIELD(&TextureInfo::width, "Width", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::height, "Height", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::depth, "Depth", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::arrayLayerCount, "ArrayLayerCount", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::mipCount, "MipCount", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::format, "Format", true, true, true);
		TYR_REFL_FIELD(&TextureInfo::type, "Type", true, true, true);
	TYR_REFL_CLASS_END();
}