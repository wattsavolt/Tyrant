#ifndef DEFERRED_LIGHTING_CS_HLSli
#define DEFERRED_LIGHTING_CS_HLSli

#include "Common.hlsli"
#include "LightUtility.hlsli"

TYR_VK_BINDING(TYR_BINDING_SCENE_INFO, 0)
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

TYR_VK_BINDING(TYR_BINDING_DIR_LIGHT, 0) StructuredBuffer<DirectionalLight> dirLights : register(t8);
TYR_VK_BINDING(TYR_BINDING_POINT_LIGHT, 0) StructuredBuffer<PointLight> pointLights : register(t9);
TYR_VK_BINDING(TYR_BINDING_SPOT_LIGHT, 0) StructuredBuffer<SpotLight> spotLights : register(t10);
// The active scene's lights, as slots in the three buffers above: directional, then point, then
// spot. Lights from other scenes stay in the buffers but aren't listed.
TYR_VK_BINDING(TYR_BINDING_LIGHT_INDICES, 0) StructuredBuffer<uint> lightIndices : register(t1, space1);

TYR_VK_BINDING(TYR_BINDING_MATERIAL, 0) StructuredBuffer<Material> materials : register(t7);
TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);
TYR_VK_BINDING(TYR_BINDING_DECAL, 0) StructuredBuffer<Decal> decals : register(t2, space1);
// The visible decals, lowest sort order first.
TYR_VK_BINDING(TYR_BINDING_DECAL_INDICES, 0) StructuredBuffer<uint> decalIndices : register(t3, space1);
// One entry per buffered RenderFrame slot - this dispatch only ever writes its own
// g_PushConstants.renderFrameIndex entry, never any other, so multiple slots' dispatches
// can safely be in flight on the GPU at once.
TYR_VK_BINDING(TYR_BINDING_LIGHTING_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u14);
// Denoised ray-traced shadow visibility, one array layer per shadow-casting light slot this
// tick - see ShadowDenoiseCS.hlsl. A directional light's position in the list is its layer;
// point/spot go through shadowLightSlotMap below instead, since there can be more of them than
// affordable shadow slots.
TYR_VK_BINDING(TYR_BINDING_SHADOW_MASKS, 0) RWTexture2DArray<float2> shadowMasks[TYR_BUFFERED_FRAME_COUNT] : register(u20);
// textures[] above is unbounded, so it claims the rest of space0's t-registers - this needs a
// distinct space, the same reason ShadowRTCS.hlsl's tlas does. The real Vulkan binding slot
// comes entirely from [[vk::binding]].
TYR_VK_BINDING(TYR_BINDING_SHADOW_LIGHT_SLOT_MAP, 0) StructuredBuffer<uint> shadowLightSlotMap : register(t0, space1);

// Matches Renderer.cpp's LightingPushConstants byte-for-byte.
struct PushConstants
{
	uint gbufferAlbedoAOIndex;
	uint gbufferNormalRoughMetalIndex;
	uint depthIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
	uint decalCount;
	// How wide a pixel is one unit from the camera.
	float pixelSizePerDistance;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

static const uint c_TileSize = 8;
static const uint c_TileThreadCount = c_TileSize * c_TileSize;
static const uint c_DecalMaskWords = TYR_MAX_DECALS / 32;
static const float c_FloatMax = 3.402823466e+38f;
// Decals fade out on surfaces turning away from them, between these cosines of the angle.
static const float c_DecalFadeStartCos = 0.5f;
static const float c_DecalFadeEndCos = 0.15f;

// The tile's depth range, as the bits of positive floats, which sort like the floats do.
groupshared uint gs_MinDepth;
groupshared uint gs_MaxDepth;
groupshared float3 gs_TileCorners[8];
// Bit i is set when visible decal i may touch the tile, so the bits keep the decals' order.
groupshared uint gs_DecalMask[c_DecalMaskWords];

float2 PixelToNDC(float2 pixel)
{
	// Row 0 is the top of the image, so NDC.y runs from +1 at row 0 down to -1.
	const float2 uv = pixel / float2(g_PushConstants.width, g_PushConstants.height);
	return float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
}

float3 NDCToWorld(float2 ndc, float deviceDepth)
{
	const float4 worldPosH = mul(float4(ndc, deviceDepth, 1.0f), InvViewProj);
	return worldPosH.xyz / worldPosH.w;
}

// Marks the visible decals whose bounds reach the box of world space the tile's pixels cover.
void CullDecals(uint3 groupId, uint groupIndex, float deviceDepth)
{
	if (groupIndex == 0)
	{
		gs_MinDepth = asuint(c_FloatMax);
		gs_MaxDepth = 0;
	}
	if (groupIndex < c_DecalMaskWords)
	{
		gs_DecalMask[groupIndex] = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	// Background pixels have nothing for a decal to land on, so they don't widen the range.
	InterlockedMin(gs_MinDepth, asuint(deviceDepth > 0.0f ? deviceDepth : c_FloatMax));
	InterlockedMax(gs_MaxDepth, asuint(deviceDepth));
	GroupMemoryBarrierWithGroupSync();

	if (groupIndex < 8)
	{
		const float2 corner = float2(groupId.xy * c_TileSize) + float2(groupIndex & 1, (groupIndex >> 1) & 1) * c_TileSize;
		// A tile of only background has its min left at the float max, so it uses the max too.
		const float minDepth = min(asfloat(gs_MinDepth), asfloat(gs_MaxDepth));
		const float depth = (groupIndex & 4) != 0 ? asfloat(gs_MaxDepth) : minDepth;
		gs_TileCorners[groupIndex] = NDCToWorld(PixelToNDC(corner), depth);
	}
	GroupMemoryBarrierWithGroupSync();

	float3 boundsMin = gs_TileCorners[0];
	float3 boundsMax = gs_TileCorners[0];
	for (uint c = 1; c < 8; ++c)
	{
		boundsMin = min(boundsMin, gs_TileCorners[c]);
		boundsMax = max(boundsMax, gs_TileCorners[c]);
	}
	const uint hasGeometry = gs_MaxDepth > 0 ? 1u : 0u;

	for (uint i = groupIndex; i < g_PushConstants.decalCount; i += c_TileThreadCount)
	{
		const Decal decal = decals[decalIndices[i]];
		const float3 outside = max(boundsMin - decal.boundsCentre, 0.0f) + max(decal.boundsCentre - boundsMax, 0.0f);
		const uint overlaps = dot(outside, outside) <= decal.boundsRadius * decal.boundsRadius ? hasGeometry : 0u;
		InterlockedOr(gs_DecalMask[i >> 5], overlaps << (i & 31));
	}
	GroupMemoryBarrierWithGroupSync();
}

// Blends a decal's material into the surface, weighted by how far inside its box the point is.
void ApplyDecal(Decal decal, float3 pixelPos, float pixelSize, float3 viewDir, inout MaterialData materialData, inout float3 normal)
{
	const float3 boxPos = mul(float4(pixelPos, 1.0f), decal.worldToBox).xyz;
	const float3 inside = step(abs(boxPos), 0.5f);
	const float depthFade = saturate((0.5f - abs(boxPos.y)) * decal.invFadeDepth);
	const float angleFade = saturate((dot(normal, decal.normal) - c_DecalFadeEndCos) / (c_DecalFadeStartCos - c_DecalFadeEndCos));

	const Material material = materials[decal.materialIndex];
	const float2 uv = float2(boxPos.x + 0.5f, 0.5f - boxPos.z) * material.uvScale + material.uvOffset;
	// A pixel's footprint in UV, which picks the mip. Wider where the surface is seen side-on.
	const float footprint = pixelSize / max(abs(dot(normal, viewDir)), 0.25f);
	const float2 gradX = float2(footprint * decal.invWidth * material.uvScale.x, 0.0f);
	const float2 gradY = float2(0.0f, footprint * decal.invLength * material.uvScale.y);

	const float4 albedoSample = textures[material.texture0].SampleGrad(samplers[0], uv, gradX, gradY);
	const float3 normalSample = textures[material.texture1].SampleGrad(samplers[0], uv, gradX, gradY).rgb;
	const float3 aoRoughMetalSample = textures[material.texture2].SampleGrad(samplers[0], uv, gradX, gradY).rgb;

	const float alpha = albedoSample.a * material.albedoTint.a * decal.opacity * inside.x * inside.y * inside.z * depthFade * angleFade;

	materialData.albedo = lerp(materialData.albedo, albedoSample.rgb * material.albedoTint.rgb, alpha * decal.colourWeight);

	// The normal map tilts the surface's own normal, so the decal follows curved surfaces.
	const float3 tangent = normalize(decal.tangent - normal * dot(decal.tangent, normal));
	const float3 bitangent = cross(normal, tangent);
	const float3 tangentSpaceNormal = (normalSample * 2.0f - 1.0f) * float3(material.normalStrength, material.normalStrength, 1.0f);
	const float3 decalNormal = normalize(mul(tangentSpaceNormal, float3x3(tangent, bitangent, normal)));
	normal = normalize(lerp(normal, decalNormal, alpha * decal.normalWeight));

	const float roughnessAlpha = alpha * decal.roughnessWeight;
	materialData.roughness = lerp(materialData.roughness, lerp(material.roughnessRange.x, material.roughnessRange.y, aoRoughMetalSample.g), roughnessAlpha);
	materialData.metallic = lerp(materialData.metallic, lerp(material.metallicRange.x, material.metallicRange.y, aoRoughMetalSample.b), roughnessAlpha);
}

[numthreads(c_TileSize, c_TileSize, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	// Pixels past the edge read the edge, so the whole group can take part in culling.
	const int3 pixelCoord = int3(min(dispatchThreadId.xy, uint2(g_PushConstants.width - 1, g_PushConstants.height - 1)), 0);
	const float deviceDepth = textures[g_PushConstants.depthIndex].Load(pixelCoord).r;

	CullDecals(groupId, groupIndex, deviceDepth);

	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	// Depth clears to 0 (reverse-Z's "infinitely far" value) - nothing was drawn here, leave
	// the background black.
	if (deviceDepth <= 0.0f)
	{
		outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

    const float4 albedoAO = textures[g_PushConstants.gbufferAlbedoAOIndex].Load(pixelCoord);
    const float4 normalRoughMetal = textures[g_PushConstants.gbufferNormalRoughMetalIndex].Load(pixelCoord);

	const float3 pixelPos = NDCToWorld(PixelToNDC(float2(dispatchThreadId.xy) + 0.5f), deviceDepth);

	float3 normal = normalize(DecodeOct(normalRoughMetal.xy));
	const float3 viewDir = normalize(camPos - pixelPos);

	MaterialData materialData;
	materialData.albedo = albedoAO.rgb;
	materialData.ambientOcclusion = albedoAO.a;
	materialData.roughness = normalRoughMetal.z;
	materialData.metallic = normalRoughMetal.w;

	// In sort order, so later decals cover earlier ones.
	const float pixelSize = distance(camPos, pixelPos) * g_PushConstants.pixelSizePerDistance;
	for (uint word = 0; word * 32 < g_PushConstants.decalCount; ++word)
	{
		uint bits = gs_DecalMask[word];
		while (bits != 0)
		{
			const uint bit = firstbitlow(bits);
			bits &= bits - 1;
			ApplyDecal(decals[decalIndices[word * 32 + bit]], pixelPos, pixelSize, viewDir, materialData, normal);
		}
	}

	float3 colour = materialData.albedo * ambient * materialData.ambientOcclusion;

	// Slot map entries hold this, meaning "not shadowed this tick" (not currently ray-traced at
	// all, or no shadow-casting light at this index) - matches Renderer.cpp's c_ShadowSlotNone.
	const uint c_ShadowSlotNone = 0xFFFFFFFFu;
	const uint3 shadowPixelCoord = uint3(dispatchThreadId.xy, 0);

	for (uint i = 0; i < dirLightCount; ++i)
	{
		// A directional light's position in the list is its shadow slot - ShadowRTCS.hlsl
		// always writes every one of them a fresh value each tick, real or a trivial "fully lit"
		// fallback, so this is always safe to sample with no further lookup.
		const float shadow = shadowMasks[g_PushConstants.renderFrameIndex][uint3(shadowPixelCoord.xy, i)].r;
		colour += ComputeDirectionalLightEffect(dirLights[lightIndices[i]], materialData, pixelPos, normal, viewDir) * shadow;
	}

	for (uint j = 0; j < pointLightCount; ++j)
	{
		const uint shadowSlot = shadowLightSlotMap[j];
		const float shadow = (shadowSlot != c_ShadowSlotNone)
			? shadowMasks[g_PushConstants.renderFrameIndex][uint3(shadowPixelCoord.xy, shadowSlot)].r
			: 1.0f;
		colour += ComputePointLightEffect(pointLights[lightIndices[TYR_MAX_DIR_LIGHTS + j]], materialData, pixelPos, normal, viewDir) * shadow;
	}

	for (uint k = 0; k < spotLightCount; ++k)
	{
		const uint shadowSlot = shadowLightSlotMap[TYR_MAX_POINT_LIGHTS + k];
		const float shadow = (shadowSlot != c_ShadowSlotNone)
			? shadowMasks[g_PushConstants.renderFrameIndex][uint3(shadowPixelCoord.xy, shadowSlot)].r
			: 1.0f;
		colour += ComputeSpotLightEffect(spotLights[lightIndices[TYR_MAX_DIR_LIGHTS + TYR_MAX_POINT_LIGHTS + k]], materialData, pixelPos, normal, viewDir) * shadow;
	}

	// This output texture is UNORM, not SRGB (SRGB doesn't support storage image usage on all
	// hardware) - write linear colour as-is; the sRGB encode happens later, same as any
	// other UI element.
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(colour, 1.0f);
}

#endif
