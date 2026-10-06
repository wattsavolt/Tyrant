#ifndef SHADOW_RT_CS_HLSli
#define SHADOW_RT_CS_HLSli

#include "Common.hlsli"

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

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
// textures[] above is unbounded, so it implicitly claims the rest of space0's t-registers -
// tlas needs a distinct space to avoid a bogus register-overlap error. The real Vulkan binding
// slot comes entirely from [[vk::binding]]; this register() annotation just needs to be unique.
TYR_VK_BINDING(TYR_BINDING_TLAS, 0) RaytracingAccelerationStructure tlas[TYR_BUFFERED_FRAME_COUNT] : register(t0, space1);
TYR_VK_BINDING(TYR_BINDING_SHADOW_MASKS_RAW, 0) RWTexture2DArray<float2> shadowMasksRaw[TYR_BUFFERED_FRAME_COUNT] : register(u20);

// Matches ShadowRTPushConstants in ShadowRTPass.cpp byte for byte.
struct PushConstants
{
	uint lightType; // 0 = Directional, 1 = Point, 2 = Spot
	uint depthIndex;
	uint normalRoughMetalIndex;
	uint outputSlot;
	uint width;
	uint height;
	uint renderFrameIndex;
	uint raysPerPixel;
	// Direction for a directional light, position for point/spot.
	float lightX;
	float lightY;
	float lightZ;
	float range;
	float spotDirX;
	float spotDirY;
	float spotDirZ;
	float spotCone;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

// Matches ShadowRTPass.h's c_ShadowLightType* constants exactly.
static const uint c_LightTypeDirectional = 0;
static const uint c_LightTypeNone = 3;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const uint3 outputCoord = uint3(dispatchThreadId.xy, g_PushConstants.outputSlot);

	// No shadow-casting light at this slot this tick - write fully lit and skip the trace
	// entirely, so a slot not currently used by any light never holds stale data from whatever
	// light last occupied it.
	if (g_PushConstants.lightType == c_LightTypeNone)
	{
		shadowMasksRaw[g_PushConstants.renderFrameIndex][outputCoord] = float2(1.0f, 0.0f);
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
	const float deviceDepth = textures[g_PushConstants.depthIndex].Load(pixelCoord).r;

	// Depth clears to 0 (reverse-Z's "infinitely far" value) - nothing was drawn here, so
	// there's nothing to shadow. Fully visible rather than fully shadowed, so an empty
	// background never looks like it's sitting in darkness.
	if (deviceDepth <= 0.0f)
	{
		shadowMasksRaw[g_PushConstants.renderFrameIndex][outputCoord] = float2(1.0f, 0.0f);
		return;
	}

	const float4 normalRoughMetal = textures[g_PushConstants.normalRoughMetalIndex].Load(pixelCoord);
	const float3 normal = normalize(DecodeOct(normalRoughMetal.xy));

	// Row 0 is the top of the image (matches this engine's flipped-viewport Y-up convention),
	// so NDC.y runs from +1 at row 0 to -1 at the last row - the opposite sign of the usual
	// "row 0 -> NDC -1" mapping.
	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	const float4 worldPosH = mul(float4(ndc, deviceDepth, 1.0f), InvViewProj);
	const float3 worldPos = worldPosH.xyz / worldPosH.w;

	float3 rayDirection;
	float rayTMax;
	if (g_PushConstants.lightType == c_LightTypeDirectional)
	{
		// A directional light has no position to aim at - trace back along its own direction,
		// far enough to clear any realistic scene extent.
		rayDirection = -normalize(float3(g_PushConstants.lightX, g_PushConstants.lightY, g_PushConstants.lightZ));
		rayTMax = 10000.0f;
	}
	else
	{
		const float3 lightPos = float3(g_PushConstants.lightX, g_PushConstants.lightY, g_PushConstants.lightZ);
		const float3 toLight = lightPos - worldPos;
		rayTMax = length(toLight);
		rayDirection = toLight / max(rayTMax, 0.0001f);
	}

	// Biased off the surface along its own normal to avoid the ray immediately re-hitting the
	// triangle it started from ("shadow acne").
	const float3 rayOrigin = worldPos + normal * 0.01f;

	RayDesc ray;
	ray.Origin = rayOrigin;
	ray.Direction = rayDirection;
	ray.TMin = 0.001f;
	ray.TMax = rayTMax;

	// Shadow rays only need a yes/no hit test, not which triangle or where exactly - cull
	// non-opaque and skip procedural primitives so the first hit found is always the answer,
	// with no need to run to completion or sort by distance.
	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(tlas[g_PushConstants.renderFrameIndex], RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	const float visibility = (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? 0.0f : 1.0f;

	shadowMasksRaw[g_PushConstants.renderFrameIndex][outputCoord] = float2(visibility, 0.0f);
}

#endif
