#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Resources/Texture.h"

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
		static constexpr uint8 c_MaxTextures = 6;
		static constexpr uint8 c_PbrAlbedoIndex = 0;
		static constexpr uint8 c_PbrNormalHeightIndex = 1;
		static constexpr uint8 c_PbrAoRoughnessMetallicIndex = 2;
	};

	struct MaterialDesc
	{
		uint index;
		MaterialType type;
		LocalArray<Texture*, MaterialConstants::c_MaxTextures> textures;
	};

	// PBR material used by the renderer 
	// Bump Map - RGB for normal and A for height
	// Ambient Occlusion / Roughness / Metallic - R = ambient occlusion, G = roughness, C = metallic
	struct Material
	{
		// Index in materual buffer in the shader
		uint index;
		// PBR:
		// Texture 0 - albedo  
		// Texture 1 - RGB for normal and A for height 
		// Texture 2 = R = ambient occlusion, G = roughness, B = metallic 
		MaterialType type;
		LocalArray<Texture*, MaterialConstants::c_MaxTextures> textures;
		// TODO : Allow custom non built-in shaders later
	};
}