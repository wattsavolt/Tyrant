#ifndef TRANSLUCENT_RT_PS_HLSli
#define TRANSLUCENT_RT_PS_HLSli

#include "Common.hlsli"
#include "LightUtility.hlsli"

// Lights translucent surfaces such as glass and adds them into two targets that are blended into
// the lit image afterwards, in any order. Each surface is weighted by how close it is, so nearer
// layers count for more.

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

TYR_VK_BINDING(TYR_BINDING_MESH, 0) StructuredBuffer<Mesh> meshes : register(t1);
TYR_VK_BINDING(TYR_BINDING_MESH_LOD, 0) StructuredBuffer<MeshLOD> meshLODs : register(t2);
TYR_VK_BINDING(TYR_BINDING_MESHLET, 0) StructuredBuffer<Meshlet> meshlets : register(t3);
TYR_VK_BINDING(TYR_BINDING_VERTEX, 0) StructuredBuffer<Vertex> vertices : register(t4);
TYR_VK_BINDING(TYR_BINDING_INDEX, 0) StructuredBuffer<uint> indices : register(t5);
TYR_VK_BINDING(TYR_BINDING_MESH_INSTANCE, 0) StructuredBuffer<MeshInstance> meshInstances : register(t6);
TYR_VK_BINDING(TYR_BINDING_MATERIAL, 0) StructuredBuffer<Material> materials : register(t7);
TYR_VK_BINDING(TYR_BINDING_DIR_LIGHT, 0) StructuredBuffer<DirectionalLight> dirLights : register(t8);
TYR_VK_BINDING(TYR_BINDING_POINT_LIGHT, 0) StructuredBuffer<PointLight> pointLights : register(t9);
TYR_VK_BINDING(TYR_BINDING_SPOT_LIGHT, 0) StructuredBuffer<SpotLight> spotLights : register(t10);
TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);
// The active scene's lights as slots in the light buffers: directional, then point, then spot.
TYR_VK_BINDING(TYR_BINDING_LIGHT_INDICES, 0) StructuredBuffer<uint> lightIndices : register(t1, space1);
// Which point and spot lights have shadows this frame.
TYR_VK_BINDING(TYR_BINDING_SHADOW_LIGHT_SLOT_MAP, 0) StructuredBuffer<uint> shadowLightSlotMap : register(t0, space1);
TYR_VK_BINDING(TYR_BINDING_TLAS, 0) RaytracingAccelerationStructure tlas[TYR_BUFFERED_FRAME_COUNT] : register(t4, space1);

TYR_VK_PUSH_CONSTANT CullPushConstants g_PushConstants;

#include "ShadowRay.hlsli"

static const uint c_ShadowSlotNone = 0xFFFFFFFFu;
// How far shadow rays start off the surface.
static const float c_ShadowBias = 0.02f;
static const float c_ShadowTMin = 0.001f;
// A directional light's rays go this far, enough to clear any scene.
static const float c_DirectionalShadowDistance = 10000.0f;
// Weights stay small enough that a few layers can't overflow half floats.
static const float c_MinWeight = 0.01f;
static const float c_MaxWeight = 300.0f;

struct TranslucentOutput
{
	// rgb = lit colour times weight, added up. a = how much of what's behind still shows,
	// multiplied together.
	float4 accumulation : SV_TARGET0;
	// Coverage times weight, added up.
	float weight : SV_TARGET1;
};

// 1 if nothing blocks the way to the light, otherwise 0. A ray no longer than its start distance
// is never blocked, which is how lights without shadows skip the trace.
float TraceShadow(float3 origin, float3 direction, float distance)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = c_ShadowTMin;
	ray.TMax = max(distance, c_ShadowTMin);

	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
	query.TraceRayInline(tlas[g_PushConstants.renderFrameIndex], RAY_FLAG_NONE, 0xFF, ray);
	while (query.Proceed())
	{
		if (BlocksLight(query.CandidateInstanceID(), query.CandidatePrimitiveIndex(), query.CandidateTriangleBarycentrics()))
		{
			query.CommitNonOpaqueTriangleHit();
		}
	}
	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0f : 1.0f;
}

// One light's diffuse and specular reflection. Diffuse is scaled by opacity, since the rest of the
// light goes through, but specular isn't, so clear glass still shows highlights.
float3 ShadeLight(MaterialData mat, float opacity, float3 normal, float3 viewDir, float3 lightDir, float3 radiance)
{
	const float3 halfVec = normalize(viewDir + lightDir);
	const float NdotL = saturate(dot(normal, lightDir));
	const float NdotV = max(dot(normal, viewDir), 1e-4f);
	const float3 F0 = lerp(c_DielectricFresnal.xxx, mat.albedo, mat.metallic);
	const float3 F = FresnelSchlick(F0, saturate(dot(halfVec, viewDir)));
	const float3 specular = DistributionGGX(normal, halfVec, mat.roughness) * GeometrySmith(normal, viewDir, lightDir, mat.roughness) * F / (4.0f * NdotV * NdotL + 1e-5f);
	const float3 diffuse = (1.0f - F) * (1.0f - mat.metallic) * mat.albedo * c_InvPi * opacity;
	return (diffuse + specular) * radiance * NdotL;
}

TranslucentOutput main(VS_OUTPUT input, bool isFrontFace : SV_IsFrontFace)
{
	// Drawn from both sides, so a back face is lit as the side facing the camera.
	const float3 normal = normalize(input.normal) * (isFrontFace ? 1.0f : -1.0f);
	const float3 tangent = normalize(input.tangent.xyz);
	const float3 bitangent = cross(normal, tangent) * input.tangent.w;
	const float3x3 TBN = float3x3(tangent, bitangent, normal);

	const Material material = materials[input.materialIndex];
	const float2 uv = input.uv * material.uvScale + material.uvOffset;

	const float3 normalSample = textures[material.texture1].Sample(samplers[0], uv).rgb;
	const float3 tangentSpaceNormal = (normalSample * 2.0f - 1.0f) * float3(material.normalStrength, material.normalStrength, 1.0f);
	const float3 surfaceNormal = normalize(mul(tangentSpaceNormal, TBN));

	const float4 albedoSample = textures[material.texture0].Sample(samplers[0], uv);
	const float3 aoRoughMetalSample = textures[material.texture2].Sample(samplers[0], uv).rgb;

	MaterialData mat;
	mat.albedo = albedoSample.rgb * material.albedoTint.rgb;
	mat.ambientOcclusion = lerp(1.0f, aoRoughMetalSample.r, material.occlusionStrength);
	mat.roughness = lerp(material.roughnessRange.x, material.roughnessRange.y, aoRoughMetalSample.g);
	mat.metallic = lerp(material.metallicRange.x, material.metallicRange.y, aoRoughMetalSample.b);
	const float opacity = saturate(albedoSample.a * material.albedoTint.a);

	const float3 position = input.worldPos.xyz;
	const float3 toCamera = camPos - position;
	const float viewDistance = length(toCamera);
	const float3 viewDir = toCamera / max(viewDistance, 1e-4f);

	// Seen side-on, the surface reflects more and lets less through.
	const float3 F0 = lerp(c_DielectricFresnal.xxx, mat.albedo, mat.metallic);
	const float3 viewFresnel = FresnelSchlick(F0, saturate(dot(surfaceNormal, viewDir)));
	const float reflectance = dot(viewFresnel, 1.0f / 3.0f);
	const float coverage = 1.0f - (1.0f - opacity) * (1.0f - reflectance);

	// Ambient light, plus the surroundings reflected as the ambient level.
	float3 colour = (mat.albedo * opacity * (1.0f - reflectance) * mat.ambientOcclusion + viewFresnel) * ambient;

	const float3 shadowOrigin = position + normal * c_ShadowBias;

	for (uint i = 0; i < dirLightCount; ++i)
	{
		const DirectionalLight light = dirLights[lightIndices[i]];
		const float3 lightDir = -light.direction;
		const float shadow = TraceShadow(shadowOrigin, lightDir, c_DirectionalShadowDistance * light.castsShadow);
		colour += ShadeLight(mat, opacity, surfaceNormal, viewDir, lightDir, light.colour * light.intensity) * shadow;
	}

	for (uint j = 0; j < pointLightCount; ++j)
	{
		const PointLight light = pointLights[lightIndices[TYR_MAX_DIR_LIGHTS + j]];
		float3 lightDir;
		float lightDistance;
		const float3 radiance = GetPointLightRadiance(light, position, lightDir, lightDistance);
		const float shadowDistance = shadowLightSlotMap[j] != c_ShadowSlotNone ? lightDistance : 0.0f;
		const float shadow = TraceShadow(shadowOrigin, lightDir, shadowDistance);
		colour += ShadeLight(mat, opacity, surfaceNormal, viewDir, lightDir, radiance) * shadow;
	}

	for (uint k = 0; k < spotLightCount; ++k)
	{
		const SpotLight light = spotLights[lightIndices[TYR_MAX_DIR_LIGHTS + TYR_MAX_POINT_LIGHTS + k]];
		float3 lightDir;
		float lightDistance;
		const float3 radiance = GetSpotLightRadiance(light, position, lightDir, lightDistance);
		const float shadowDistance = shadowLightSlotMap[TYR_MAX_POINT_LIGHTS + k] != c_ShadowSlotNone ? lightDistance : 0.0f;
		const float shadow = TraceShadow(shadowOrigin, lightDir, shadowDistance);
		colour += ShadeLight(mat, opacity, surfaceNormal, viewDir, lightDir, radiance) * shadow;
	}

	// Nearer surfaces get far more weight, so they end up in front.
	const float distanceWeight = 10.0f / (1e-5f + pow(viewDistance / 5.0f, 2.0f) + pow(viewDistance / 200.0f, 6.0f));
	const float weight = coverage * clamp(distanceWeight, c_MinWeight, c_MaxWeight);

	TranslucentOutput output;
	output.accumulation = float4(colour * weight, coverage);
	output.weight = coverage * weight;
	return output;
}

#endif
