#ifndef GBUFFER_PS_HLSli
#define GBUFFER_PS_HLSli

#include "Common.hlsli"

// camPos (parallax view direction) and PrevViewProj (motion vectors, below) are the only
// fields actually used here - everything in between exists purely so PrevViewProj lands
// at its real cbuffer offset.
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
	float _pad0;
	float4 _frustumPlanes[6];
	float4x4 PrevViewProj;
	float2 JitterDelta;
};

[[vk::binding(TYR_BINDING_MATERIAL, 0)]] StructuredBuffer<Material> materials : register(t7);
[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
[[vk::binding(TYR_BINDING_SAMPLERS, 0)]] SamplerState samplers[] : register(s12);

struct GBufferOutput
{
	// rgb = albedo, a = ambient occlusion
	float4 albedoAO : SV_TARGET0;
	// rg = octahedral-encoded world-space normal, b = roughness, a = metallic
	float4 normalRoughMetal : SV_TARGET1;
	// Screen-space motion vector: current NDC.xy minus reprojected previous-frame NDC.xy of
	// this same world position. Camera motion only for now.
	float2 motion : SV_TARGET2;
};

// How far a texel's height (relative to 0.5, the neutral/no-height-data value) shifts the
// UV used to sample this surface point, per unit of tangent-space view direction. Small
// and constant for now - single-sample offset, not steep/relief parallax.
static const float c_HeightScale = 0.03f;

GBufferOutput main(VS_OUTPUT input)
{
	const float3 normal = normalize(input.normal);
	const float3 tangent = normalize(input.tangent.xyz);
	const float3 bitangent = cross(normal, tangent) * input.tangent.w;
	const float3x3 TBN = float3x3(tangent, bitangent, normal);

	const float3 pixelPos = input.worldPos.xyz;
	const float3 viewDir = normalize(camPos - pixelPos);

	// materialIndex is never invalid here - a mesh instance's renderer-side handle, and so
	// any draw of it, doesn't exist until every one of its submeshes' materials is fully
	// loaded and uploaded.
	const Material material = materials[input.materialIndex];

	// Single texture holds normal (rgb) + height (a). Sampled once at the untouched UV to get
	// this point's height, which then offsets every other sample the same way real parallax
	// would - a flat/absent height map (0.5 everywhere) offsets by exactly zero.
	const float3 viewDirTS = mul(TBN, viewDir);
	const float height = textures[material.texture1].Sample(samplers[0], input.uv).a;
	const float2 uv = input.uv - viewDirTS.xy * ((height - 0.5f) * c_HeightScale);

	const float3 normalSample = textures[material.texture1].Sample(samplers[0], uv).rgb;
	const float3 tangentSpaceNormal = normalSample * 2.0f - 1.0f;
	const float3 perturbedNormal = normalize(mul(tangentSpaceNormal, TBN));

	const float3 albedo = textures[material.texture0].Sample(samplers[0], uv).rgb;
	const float3 aoRoughMetal = textures[material.texture2].Sample(samplers[0], uv).rgb;

	// Perspective-divide both this frame's and last frame's clip-space position of the same
	// world point to get their NDC.xy, then take the difference. ViewProj/PrevViewProj both
	// carry this tick's/last tick's own TAA jitter baked in (see Renderer.cpp), so the raw
	// difference also carries a spurious jitter-delta term - subtracting JitterDelta removes it,
	// leaving only real scene motion. A no-op (JitterDelta is zero) whenever TAA is disabled.
	const float4 currentClip = mul(input.worldPos, ViewProj);
	const float4 previousClip = mul(input.worldPos, PrevViewProj);
	const float2 motion = ((currentClip.xy / currentClip.w) - (previousClip.xy / previousClip.w)) - JitterDelta;

	GBufferOutput output;
	output.albedoAO = float4(albedo, aoRoughMetal.r);
	output.normalRoughMetal = float4(EncodeOct(perturbedNormal), aoRoughMetal.g, aoRoughMetal.b);
	output.motion = motion;
	return output;
}

#endif
