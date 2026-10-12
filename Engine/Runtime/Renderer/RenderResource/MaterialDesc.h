#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

namespace tyr
{
	enum class MaterialType : uint8
	{
		PBR,
		Hair,
		Particle,
		Custom
	};

	// How a material's surface covers what's behind it, as glTF's alpha modes.
	enum class MaterialBlendMode : uint8
	{
		Opaque,
		// Solid where albedo alpha is at least alphaCutoff and a hole elsewhere, such as a fence.
		Masked,
		// Blended over what's behind it by albedo alpha, such as glass.
		Translucent
	};

	struct MaterialConstants
	{
		static constexpr uint8 c_MaxTextures = 4;
		static constexpr uint8 c_PbrAlbedoIndex = 0;
		static constexpr uint8 c_PbrNormalHeightIndex = 1;
		static constexpr uint8 c_PbrAoRoughnessMetallicIndex = 2;
	};

	// Adjusts how a material shows its textures, so one texture set can make several materials.
	// Matches glTF: the tint and strengths multiply, and roughness and metallic map the texture's
	// 0 to 1 onto their min to max, so equal values give a constant without a texture.
	struct MaterialParams
	{
		// rgb multiplies albedo, a multiplies albedo alpha.
		Vector4 albedoTint = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		Vector2 uvScale = Vector2(1.0f, 1.0f);
		Vector2 uvOffset = Vector2(0.0f, 0.0f);
		float roughnessMin = 0.0f;
		float roughnessMax = 1.0f;
		float metallicMin = 0.0f;
		float metallicMax = 1.0f;
		float normalStrength = 1.0f;
		float occlusionStrength = 1.0f;
		// How far the height map shifts the UVs for parallax, in UV units.
		float heightScale = 0.03f;
		// Masked only.
		float alphaCutoff = 0.5f;
		// A MaterialBlendMode: 0 opaque, 1 masked, 2 translucent.
		uint8 blendMode = static_cast<uint8>(MaterialBlendMode::Opaque);
	};

	struct MaterialDesc
	{
		LocalArray<Handle, MaterialConstants::c_MaxTextures> textures;
		MaterialType type;
		MaterialParams params;
	};
}
