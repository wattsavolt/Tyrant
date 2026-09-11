#pragma once

#include "MaterialDesc.h"

namespace tyr
{
	// PBR material used by the renderer 
	// Bump Map - RGB for normal and A for height
	// Ambient Occlusion / Roughness / Metallic - R = ambient occlusion, G = roughness, C = metallic
	struct Material
	{
		LocalArray<Handle, MaterialConstants::c_MaxTextures> textures;
		// PBR:
		// Texture 0 - albedo  
		// Texture 1 - RGB for normal and A for height 
		// Texture 2 - R = ambient occlusion, G = roughness, B = metallic 
		MaterialType type;
		// TODO : Allow custom non built-in shaders later
	};
}