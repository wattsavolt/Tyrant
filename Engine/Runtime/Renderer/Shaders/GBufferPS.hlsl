#ifndef GBUFFER_PS_HLSli
#define GBUFFER_PS_HLSli

#include "Common.hlsli"

// The masked pass (TYR_MASKED_PASS 1) cuts out pixels below the alpha cutoff and is drawn from
// both sides. It's a separate pipeline so the opaque pass keeps early depth testing.
#ifndef TYR_MASKED_PASS
#define TYR_MASKED_PASS 0
#endif

// camPos (parallax view direction) and PrevViewProj (motion vectors, below) are the only
// fields actually used here - everything in between exists purely so PrevViewProj lands
// at its real cbuffer offset.
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
	float _pad0;
	float4 _frustumPlanes[6];
	float4x4 PrevViewProj;
	float2 JitterDelta;
};

TYR_VK_BINDING(TYR_BINDING_MATERIAL, 0) StructuredBuffer<Material> materials : register(t7);
TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);

// Smooth noise from 0 to 1 that changes about once per unit.
float ValueNoise(float2 p)
{
	const float2 cell = floor(p);
	const float2 f = p - cell;
	const float2 u = f * f * (3.0f - 2.0f * f);
	const float4 corners = cell.xyxy + float4(0.0f, 0.0f, 1.0f, 1.0f);
	const float4 hashes = frac(sin(float4(dot(corners.xy, float2(127.1f, 311.7f)), dot(corners.zy, float2(127.1f, 311.7f)),
		dot(corners.xw, float2(127.1f, 311.7f)), dot(corners.zw, float2(127.1f, 311.7f)))) * 43758.5453f);
	return lerp(lerp(hashes.x, hashes.y, u.x), lerp(hashes.z, hashes.w, u.x), u.y);
}

// How much the layer covers this point, from patches of noise across the ground.
float LayerWeight(float3 worldPos, Material material)
{
	const float2 p = worldPos.xz / max(material.layerPatchSize, 0.01f);
	const float noise = ValueNoise(p) * 0.65f + ValueNoise(p * 2.7f + 17.3f) * 0.35f;
	return saturate((noise - (1.0f - material.layerCoverage)) * material.layerSharpness + 0.5f);
}

struct GBufferOutput
{
	// rgb = albedo, a = ambient occlusion
	float4 albedoAO : SV_TARGET0;
	// rg = octahedral-encoded world-space normal, b = roughness, a = metallic
	float4 normalRoughMetal : SV_TARGET1;
	// Screen-space motion vector: current NDC.xy minus reprojected previous-frame NDC.xy of
	// this same world position. Camera and skinned vertex motion only for now.
	float2 motion : SV_TARGET2;
	// Light the surface gives off, added by the lighting pass.
	float4 emissive : SV_TARGET3;
};

#if TYR_MASKED_PASS
GBufferOutput main(VS_OUTPUT input, bool isFrontFace : SV_IsFrontFace)
#else
GBufferOutput main(VS_OUTPUT input)
#endif
{
#if TYR_MASKED_PASS
	// A back face is lit as the side facing the camera.
	const float3 normal = normalize(input.normal) * (isFrontFace ? 1.0f : -1.0f);
#else
	const float3 normal = normalize(input.normal);
#endif
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
	const float2 baseUV = input.uv * material.uvScale + material.uvOffset;
	const float3 viewDirTS = mul(TBN, viewDir);
	const float height = textures[material.texture1].Sample(samplers[0], baseUV).a;
	const float2 uv = baseUV - viewDirTS.xy * ((height - 0.5f) * material.heightScale);

	float3 normalSample = textures[material.texture1].Sample(samplers[0], uv).rgb;
	float4 albedoSample = textures[material.texture0].Sample(samplers[0], uv);
	float3 aoRoughMetalSample = textures[material.texture2].Sample(samplers[0], uv).rgb;

	// A layer, when the material has one, is blended over the base in patches.
	if (material.texture3 != TYR_INVALID_INDEX)
	{
		const float2 layerUV = input.uv * material.layerUVScale;
		const float weight = LayerWeight(pixelPos, material);
		normalSample = lerp(normalSample, textures[material.texture4].Sample(samplers[0], layerUV).rgb, weight);
		albedoSample = lerp(albedoSample, textures[material.texture3].Sample(samplers[0], layerUV), weight);
		aoRoughMetalSample = lerp(aoRoughMetalSample, textures[material.texture5].Sample(samplers[0], layerUV).rgb, weight);
		// Broad, gentle changes in brightness hide the textures repeating across large areas.
		const float broad = ValueNoise(pixelPos.xz / max(material.layerPatchSize * 5.0f, 0.01f) + 41.7f);
		albedoSample.rgb *= lerp(0.8f, 1.12f, broad);
	}

	// Strength scales the normal's tilt away from the surface, as glTF's normal scale does.
	const float3 tangentSpaceNormal = (normalSample * 2.0f - 1.0f) * float3(material.normalStrength, material.normalStrength, 1.0f);
	const float3 perturbedNormal = normalize(mul(tangentSpaceNormal, TBN));

#if TYR_MASKED_PASS
	clip(albedoSample.a * material.albedoTint.a - material.alphaCutoff);
#endif
	const float3 albedo = albedoSample.rgb * material.albedoTint.rgb;
	const float ao = lerp(1.0f, aoRoughMetalSample.r, material.occlusionStrength);
	const float roughness = lerp(material.roughnessRange.x, material.roughnessRange.y, aoRoughMetalSample.g);
	const float metallic = lerp(material.metallicRange.x, material.metallicRange.y, aoRoughMetalSample.b);

	// Perspective-divide both this frame's and last frame's clip-space position of the same
	// world point to get their NDC.xy, then take the difference. ViewProj/PrevViewProj both
	// carry this tick's/last tick's own TAA jitter baked in (see Renderer.cpp), so the raw
	// difference also carries a spurious jitter-delta term - subtracting JitterDelta removes it,
	// leaving only real scene motion. A no-op (JitterDelta is zero) whenever TAA is disabled.
	const float4 currentClip = mul(input.worldPos, ViewProj);
	const float4 previousClip = mul(input.prevWorldPos, PrevViewProj);
	const float2 motion = ((currentClip.xy / currentClip.w) - (previousClip.xy / previousClip.w)) - JitterDelta;

	GBufferOutput output;
	output.albedoAO = float4(albedo, ao);
	output.normalRoughMetal = float4(EncodeOct(perturbedNormal), roughness, metallic);
	output.motion = motion;
	output.emissive = float4(material.emissive * input.emissiveScale, 0.0f);
	return output;
}

#endif
