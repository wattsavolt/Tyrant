#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	enum class MaterialType : uint8
	{
		PBR,
		Hair,
		Particle,
		Custom
	};

	struct MaterialConstants
	{
		static constexpr uint8 c_MaxTextures = 4;
		static constexpr uint8 c_PbrAlbedoIndex = 0;
		static constexpr uint8 c_PbrNormalHeightIndex = 1;
		static constexpr uint8 c_PbrAoRoughnessMetallicIndex = 2;
	};

	struct MaterialDesc
	{
		LocalArray<Handle, MaterialConstants::c_MaxTextures> textures;
		MaterialType type;
	};
}