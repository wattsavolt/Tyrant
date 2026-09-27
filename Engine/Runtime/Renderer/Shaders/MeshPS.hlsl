#ifndef MESH_PS_HLSli
#define MESH_PS_HLSli

#include "Common.hlsli"
#include "LightUtility.hlsli"

[[vk::binding(TYR_BINDING_SCENE_INFO, 0)]]
cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float3 camPos;
	float ambient;
	// How many of dirLights/pointLights/spotLights' entries (from index 0) are real - see
	// SceneInfo::dirLightCount's own comment (Shaders/ShaderTypes.h). Each buffer is sized for
	// its own RenderConstants::c_Max*Lights, but looping that fixed count instead of these
	// would read whichever slots nothing has ever written to - not guaranteed to be zeroed.
	uint dirLightCount;
	uint pointLightCount;
	uint spotLightCount;
};

[[vk::binding(TYR_BINDING_MATERIAL, 0)]] StructuredBuffer<Material> materials : register(t7);
[[vk::binding(TYR_BINDING_DIR_LIGHT, 0)]] StructuredBuffer<DirectionalLight> dirLights : register(t8);
[[vk::binding(TYR_BINDING_POINT_LIGHT, 0)]] StructuredBuffer<PointLight> pointLights : register(t9);
[[vk::binding(TYR_BINDING_SPOT_LIGHT, 0)]] StructuredBuffer<SpotLight> spotLights : register(t10);

[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
[[vk::binding(TYR_BINDING_SAMPLERS, 0)]] SamplerState samplers[] : register(s12);

// How far a texel's height (relative to 0.5, the neutral/no-height-data value - see
// MaterialImporter::CreateNormalHeight) shifts the UV used to sample this surface point,
// per unit of tangent-space view direction. Small and constant rather than tuned per
// material for now - single-sample offset, not steep/relief parallax.
static const float c_HeightScale = 0.03f;

float4 main(VS_OUTPUT input) : SV_TARGET
{
	const float3 normal = normalize(input.normal);
	const float3 tangent = normalize(input.tangent.xyz);
	const float3 bitangent = cross(normal, tangent) * input.tangent.w;
	const float3x3 TBN = float3x3(tangent, bitangent, normal);

	const float3 pixelPos = input.worldPos.xyz;
	const float3 viewDir = normalize(camPos - pixelPos);

	// materialIndex is never c_InvalidIndex here - a mesh instance's renderer-side handle,
	// and so any draw of it, doesn't exist until AssetManager has every one of its
	// submeshes' materials fully loaded and uploaded (see
	// AssetManager::TryResolvePendingMeshInstances).
	const Material material = materials[input.materialIndex];

	// Single texture holds normal (rgb) + height (a) - see MaterialImporter::CreateNormalHeight.
	// Sampled once at the untouched UV to get this point's height, which then offsets every
	// other sample (including the normal's own, below) the same way real parallax would -
	// a flat/absent height map (0.5 everywhere) offsets by exactly zero.
	const float3 viewDirTS = mul(TBN, viewDir);
	const float height = textures[material.texture1].Sample(samplers[0], input.uv).a;
	const float2 uv = input.uv - viewDirTS.xy * ((height - 0.5f) * c_HeightScale);

	const float3 normalSample = textures[material.texture1].Sample(samplers[0], uv).rgb;
	const float3 tangentSpaceNormal = normalSample * 2.0f - 1.0f;
	const float3 perturbedNormal = normalize(mul(tangentSpaceNormal, TBN));

	MaterialData materialData;
	materialData.albedo = textures[material.texture0].Sample(samplers[0], uv).rgb;
	const float3 aoRoughMetal = textures[material.texture2].Sample(samplers[0], uv).rgb;
	materialData.ambientOcclusion = aoRoughMetal.r;
	materialData.roughness = aoRoughMetal.g;
	materialData.metallic = aoRoughMetal.b;

	float3 colour = materialData.albedo * ambient * materialData.ambientOcclusion;

	for (uint i = 0; i < dirLightCount; ++i)
	{
		colour += ComputeDirectionalLightEffect(dirLights[i], materialData, pixelPos, perturbedNormal, viewDir);
	}

	for (uint j = 0; j < pointLightCount; ++j)
	{
		colour += ComputePointLightEffect(pointLights[j], materialData, pixelPos, perturbedNormal, viewDir);
	}

	for (uint k = 0; k < spotLightCount; ++k)
	{
		colour += ComputeSpotLightEffect(spotLights[k], materialData, pixelPos, perturbedNormal, viewDir);
	}

	// No need to convert to sRGB manually as the swapchain format is sRGB so the linear
	// colour is automatically converted to sRGB.
	return float4(colour, 1.0f);
}

#endif
