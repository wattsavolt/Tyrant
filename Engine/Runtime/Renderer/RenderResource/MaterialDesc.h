#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
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
		static constexpr uint8 c_MaxTextures = 6;
		static constexpr uint8 c_PbrAlbedoIndex = 0;
		static constexpr uint8 c_PbrNormalHeightIndex = 1;
		static constexpr uint8 c_PbrAoRoughnessMetallicIndex = 2;
		// An optional second texture set, blended over the first in patches, such as grass over gravel.
		static constexpr uint8 c_PbrLayerAlbedoIndex = 3;
		static constexpr uint8 c_PbrLayerNormalHeightIndex = 4;
		static constexpr uint8 c_PbrLayerAoRoughnessMetallicIndex = 5;
		static constexpr uint8 c_PbrLayerTextureCount = 3;
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
		// The layer's textures repeat this many times per UV unit.
		Vector2 layerUVScale = Vector2(1.0f, 1.0f);
		// Roughly how wide the layer's patches are, in metres.
		float layerPatchSize = 6.0f;
		// How much of the surface the layer covers, from 0 to 1.
		float layerCoverage = 0.5f;
		// How quickly a patch's edge blends into the base, higher being harder.
		float layerSharpness = 4.0f;
		// Light the surface gives off itself, such as a lamp's diffuser, in linear colour.
		Vector3 emissiveColour = Vector3(0.0f, 0.0f, 0.0f);
		float emissiveIntensity = 1.0f;
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
