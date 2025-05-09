#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Resources/Texture.h"

namespace tyr
{
	// PBR material used by the renderer 
	// Bump Map - RGB for normal and A for height
	// Ambient Occlusion / Roughness / Metallic - R = ambient occlusion, G = roughness, C = metallic
	struct Material
	{
		AssetID id;
		Ref<Texture> albedo;
		// RGB for normal and A for height
		Ref<Texture> bumpMap;
		// R = ambient occlusion, G = roughness, C = metallic
		Ref<Texture> aoRoughnessMetallic;
	};
}