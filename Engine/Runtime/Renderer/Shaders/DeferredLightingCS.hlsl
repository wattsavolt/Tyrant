#ifndef DEFERRED_LIGHTING_CS_HLSli
#define DEFERRED_LIGHTING_CS_HLSli

#include "Common.hlsli"
#include "LightUtility.hlsli"

[[vk::binding(TYR_BINDING_SCENE_INFO, 0)]]
cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3 camPos;
	float ambient;
	uint dirLightCount;
	uint pointLightCount;
	uint spotLightCount;
};

[[vk::binding(TYR_BINDING_DIR_LIGHT, 0)]] StructuredBuffer<DirectionalLight> dirLights : register(t8);
[[vk::binding(TYR_BINDING_POINT_LIGHT, 0)]] StructuredBuffer<PointLight> pointLights : register(t9);
[[vk::binding(TYR_BINDING_SPOT_LIGHT, 0)]] StructuredBuffer<SpotLight> spotLights : register(t10);

[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
// One entry per buffered RenderFrame slot - this dispatch only ever writes its own
// g_PushConstants.renderFrameIndex entry, never any other, so multiple slots' dispatches
// can safely be in flight on the GPU at once.
[[vk::binding(TYR_BINDING_LIGHTING_OUTPUT, 0)]] RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u14);

// Matches Renderer.cpp's LightingPushConstants byte-for-byte.
struct PushConstants
{
	uint gbufferAlbedoAOIndex;
	uint gbufferNormalRoughMetalIndex;
	uint depthIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
};
[[vk::push_constant]] PushConstants g_PushConstants;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
    const float deviceDepth = textures[g_PushConstants.depthIndex].Load(pixelCoord).r;

	// Depth clears to 0 (reverse-Z's "infinitely far" value) - nothing was drawn here, leave
	// the background black.
	if (deviceDepth <= 0.0f)
	{
		outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

    const float4 albedoAO = textures[g_PushConstants.gbufferAlbedoAOIndex].Load(pixelCoord);
    const float4 normalRoughMetal = textures[g_PushConstants.gbufferNormalRoughMetalIndex].Load(pixelCoord);

	// Row 0 is the top of the image (matches this engine's flipped-viewport Y-up convention),
	// so NDC.y runs from +1 at row 0 to -1 at the last row - the opposite sign of the usual
	// "row 0 -> NDC -1" mapping.
	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	const float4 worldPosH = mul(float4(ndc, deviceDepth, 1.0f), InvViewProj);
	const float3 pixelPos = worldPosH.xyz / worldPosH.w;

	const float3 normal = normalize(DecodeOct(normalRoughMetal.xy));
	const float3 viewDir = normalize(camPos - pixelPos);

	MaterialData materialData;
	materialData.albedo = albedoAO.rgb;
	materialData.ambientOcclusion = albedoAO.a;
	materialData.roughness = normalRoughMetal.z;
	materialData.metallic = normalRoughMetal.w;

	float3 colour = materialData.albedo * ambient * materialData.ambientOcclusion;

	for (uint i = 0; i < dirLightCount; ++i)
	{
		colour += ComputeDirectionalLightEffect(dirLights[i], materialData, pixelPos, normal, viewDir);
	}

	for (uint j = 0; j < pointLightCount; ++j)
	{
		colour += ComputePointLightEffect(pointLights[j], materialData, pixelPos, normal, viewDir);
	}

	for (uint k = 0; k < spotLightCount; ++k)
	{
		colour += ComputeSpotLightEffect(spotLights[k], materialData, pixelPos, normal, viewDir);
	}

	// This output texture is UNORM, not SRGB (SRGB doesn't support storage image usage on all
	// hardware) - write linear colour as-is; the sRGB encode happens later, same as any
	// other UI element.
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(colour, 1.0f);
}

#endif
