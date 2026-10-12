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
	// The most a drawn LOD can stray from LOD0 per unit of distance from the camera.
	float lodErrorPerDistance;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

// Matches ShadowRTPass.h's c_ShadowLightType* constants exactly.
static const uint c_LightTypeDirectional = 0;
static const uint c_LightTypeNone = 3;

// The ray starts this many times the drawn LOD's furthest possible error off the surface.
static const float c_LodErrorBiasScale = 2.0f;
static const float c_MinBias = 0.01f;
// Back faces are ignored for this many times the bias from the ray's origin.
static const float c_BackFaceCullBiasScale = 8.0f;

float3 ReconstructWorldPos(int2 pixel, float deviceDepth)
{
	// Row 0 is the top of the image (matches this engine's flipped-viewport Y-up convention),
	// so NDC.y runs from +1 at row 0 to -1 at the last row.
	const float2 uv = (float2(pixel) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	const float4 worldPosH = mul(float4(ndc, deviceDepth, 1.0f), InvViewProj);
	return worldPosH.xyz / worldPosH.w;
}

// The step along one screen axis to whichever neighbour is nearer in depth, so both ends stay on
// the same surface at an edge. A neighbour clamped back onto this pixel at the border isn't used.
float3 SurfaceStep(int2 pixel, int2 step, float deviceDepth, float3 worldPos)
{
	const int2 last = int2(g_PushConstants.width, g_PushConstants.height) - 1;
	const int2 forwardPixel = clamp(pixel + step, 0, last);
	const int2 backPixel = clamp(pixel - step, 0, last);
	const float forwardDepth = textures[g_PushConstants.depthIndex].Load(int3(forwardPixel, 0)).r;
	const float backDepth = textures[g_PushConstants.depthIndex].Load(int3(backPixel, 0)).r;
	const float forwardDifference = all(forwardPixel == pixel) ? 3.402823e38f : abs(forwardDepth - deviceDepth);
	const float backDifference = all(backPixel == pixel) ? 3.402823e38f : abs(backDepth - deviceDepth);

	const float3 forwardStep = ReconstructWorldPos(forwardPixel, forwardDepth) - worldPos;
	const float3 backStep = worldPos - ReconstructWorldPos(backPixel, backDepth);
	return forwardDifference <= backDifference ? forwardStep : backStep;
}

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

	const int2 pixel = int2(dispatchThreadId.xy);
	const float3 worldPos = ReconstructWorldPos(pixel, deviceDepth);
	const float3 toCamera = camPos - worldPos;

	// The flat surface's normal, from the neighbouring depths, so the ray's offset follows the
	// drawn triangles rather than a normal map. Turned to face the camera, since that side was drawn.
	// Where the neighbours span no area, the G-buffer's normal stands in.
	const float3 crossed = cross(SurfaceStep(pixel, int2(1, 0), deviceDepth, worldPos), SurfaceStep(pixel, int2(0, 1), deviceDepth, worldPos));
	const float crossedLength = length(crossed);
	const float3 flatNormal = crossed / max(crossedLength, 1e-20f) * (dot(crossed, toCamera) < 0.0f ? -1.0f : 1.0f);
	const float3 shadingNormal = normalize(DecodeOct(textures[g_PushConstants.normalRoughMetalIndex].Load(pixelCoord).xy));
	const float3 normal = crossedLength > 1e-12f ? flatNormal : shadingNormal;

	// A directional light has no position to aim at - trace back along its own direction, far
	// enough to clear any realistic scene extent. Both are worked out and one picked.
	const float3 light = float3(g_PushConstants.lightX, g_PushConstants.lightY, g_PushConstants.lightZ);
	const bool directional = g_PushConstants.lightType == c_LightTypeDirectional;
	const float3 toLight = light - worldPos;
	const float toLightDistance = length(toLight);
	const float3 rayDirection = directional ? -normalize(light) : toLight / max(toLightDistance, 0.0001f);
	const float rayTMax = directional ? 10000.0f : toLightDistance;

	// The acceleration structures hold LOD0, which can be up to lodErrorPerDistance * distance from
	// the drawn LOD's surface, so the origin is lifted past that to stop the ray hitting LOD0 under
	// its own pixel ("shadow acne"). Depth precision also falls off with distance.
	const float bias = max(c_MinBias, c_LodErrorBiasScale * g_PushConstants.lodErrorPerDistance * length(toCamera));

	RayDesc ray;
	ray.Origin = worldPos + normal * bias;
	ray.Direction = rayDirection;
	ray.TMin = 0.001f;
	ray.TMax = rayTMax;

	// Near the origin, back faces are skipped - a ray that still starts under LOD0's surface leaves
	// through a back face. Past that they block light, so a single-sided wall still casts shadows.
	const float cullDistance = clamp(c_BackFaceCullBiasScale * bias, ray.TMin, rayTMax);
	RayDesc nearRay = ray;
	nearRay.TMax = cullDistance;
	RayDesc farRay = ray;
	farRay.TMin = cullDistance;

	// Shadow rays only need a yes/no hit test, so the first opaque hit found is the answer.
	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> nearQuery;
	nearQuery.TraceRayInline(tlas[g_PushConstants.renderFrameIndex], RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, nearRay);
	nearQuery.Proceed();

	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> farQuery;
	farQuery.TraceRayInline(tlas[g_PushConstants.renderFrameIndex], RAY_FLAG_NONE, 0xFF, farRay);
	farQuery.Proceed();

	const bool blocked = or(nearQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT, farQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
	const float visibility = blocked ? 0.0f : 1.0f;

	shadowMasksRaw[g_PushConstants.renderFrameIndex][outputCoord] = float2(visibility, 0.0f);
}

#endif
