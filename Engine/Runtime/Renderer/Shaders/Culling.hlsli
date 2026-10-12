#ifndef CULLING_HLSli
#define CULLING_HLSli

#include "Common.hlsli"
#include "HiZ.hlsli"

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
	float4 FrustumPlanes[6];
};

TYR_VK_BINDING(TYR_BINDING_HIZ, 0) StructuredBuffer<float> hizLevels[TYR_BUFFERED_FRAME_COUNT] : register(t0, space2);

TYR_VK_PUSH_CONSTANT CullPushConstants g_PushConstants;

// Culled if the sphere is fully on the outside of any plane - dot(pos,abc)+d >= 0 means
// "inside" that plane. Every plane is tested, keeping the closest, so there's no branching.
bool IsSphereInFrustum(float3 centre, float radius)
{
	float closest = 3.402823e38f;
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		const float4 plane = FrustumPlanes[i];
		closest = min(closest, dot(centre, plane.xyz) + plane.w + radius);
	}
	return closest >= 0.0f;
}

// Same idea as IsSphereInFrustum, for a world-space AABB given as centre + half-extents (Arvo's
// method) - the box's "radius" along a plane's normal is dot(abs(plane.xyz), halfExtents).
bool IsBoxInFrustum(float3 centre, float3 halfExtents)
{
	float closest = 3.402823e38f;
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		const float4 plane = FrustumPlanes[i];
		closest = min(closest, dot(centre, plane.xyz) + plane.w + dot(abs(plane.xyz), halfExtents));
	}
	return closest >= 0.0f;
}

// The largest scale the transform applies along any axis, squared.
float MaxScaleSqr(float4x4 transform)
{
	const float3 row0 = float3(transform._11, transform._12, transform._13);
	const float3 row1 = float3(transform._21, transform._22, transform._23);
	const float3 row2 = float3(transform._31, transform._32, transform._33);
	return max(dot(row0, row0), max(dot(row1, row1), dot(row2, row2)));
}

// False only if everything already drawn this frame is in front of the whole box. A box crossing
// the near plane can't be projected, so it always counts as visible.
bool IsBoxVisibleInHiZ(float3 centre, float3 halfExtents)
{
	float2 ndcMin = float2(3.402823e38f, 3.402823e38f);
	float2 ndcMax = -ndcMin;
	float nearestDepth = 0.0f;
	float minW = 3.402823e38f;
	[unroll]
	for (uint i = 0; i < 8; ++i)
	{
		const float3 cornerSign = float3(uint3(i, i >> 1, i >> 2) & 1u) * 2.0f - 1.0f;
		const float3 corner = centre + halfExtents * cornerSign;
		const float4 clip = mul(float4(corner, 1.0f), ViewProj);
		const float3 ndc = clip.xyz / clip.w;
		minW = min(minW, clip.w);
		ndcMin = min(ndcMin, ndc.xy);
		ndcMax = max(ndcMax, ndc.xy);
		// Reverse-Z, so the nearest corner has the largest depth.
		nearestDepth = max(nearestDepth, ndc.z);
	}

	// Inclusive depth pixel range the box covers. Screen y runs down while NDC y runs up.
	const uint2 depthSize = uint2(g_PushConstants.depthWidth, g_PushConstants.depthHeight);
	const float2 uvMin = saturate(float2(ndcMin.x, -ndcMax.y) * 0.5f + 0.5f);
	const float2 uvMax = saturate(float2(ndcMax.x, -ndcMin.y) * 0.5f + 0.5f);
	const uint2 pixelMin = min((uint2)(uvMin * (float2)depthSize), depthSize - 1u);
	const uint2 pixelMax = min((uint2)(uvMax * (float2)depthSize), depthSize - 1u);

	// The finest level where the range spans at most 2x2 texels, since a span under 2^(L+1) pixels
	// can't touch more than two texels of that size.
	const uint2 span = pixelMax - pixelMin;
	const uint level = min(firstbithigh(max(max(span.x, span.y), 1u)), HiZLevelCount(depthSize) - 1u);
	const uint2 levelSize = HiZLevelSize(depthSize, level);
	const uint offset = HiZLevelOffset(depthSize, level);
	const uint2 texelMin = min(pixelMin >> (level + 1u), levelSize - 1u);
	const uint2 texelMax = min(pixelMax >> (level + 1u), levelSize - 1u);

	const uint frame = g_PushConstants.renderFrameIndex;
	const float farthest = min(
		min(hizLevels[frame][offset + texelMin.y * levelSize.x + texelMin.x], hizLevels[frame][offset + texelMin.y * levelSize.x + texelMax.x]),
		min(hizLevels[frame][offset + texelMax.y * levelSize.x + texelMin.x], hizLevels[frame][offset + texelMax.y * levelSize.x + texelMax.x]));

	return or(nearestDepth >= farthest, minW <= 0.0f);
}

#endif
