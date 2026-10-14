#include "MaterialDesc.h"

namespace tyr
{
	TYR_REFL_CLASS_START(MaterialType, 0);
	TYR_REFL_CLASS_END();

	TYR_REFL_CLASS_START(MaterialParams, 0);
		TYR_REFL_FIELD(&MaterialParams::blendMode, "Blend Mode", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::albedoTint, "Albedo Tint", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::uvScale, "UV Scale", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::uvOffset, "UV Offset", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::roughnessMin, "Roughness Min", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::roughnessMax, "Roughness Max", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::metallicMin, "Metallic Min", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::metallicMax, "Metallic Max", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::normalStrength, "Normal Strength", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::occlusionStrength, "Occlusion Strength", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::heightScale, "Height Scale", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::alphaCutoff, "Alpha Cutoff", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::layerUVScale, "Layer UV Scale", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::layerPatchSize, "Layer Patch Size", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::layerCoverage, "Layer Coverage", true, true, true);
		TYR_REFL_FIELD(&MaterialParams::layerSharpness, "Layer Sharpness", true, true, true);
	TYR_REFL_CLASS_END();
}
