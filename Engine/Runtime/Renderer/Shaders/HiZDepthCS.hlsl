#ifndef HIZ_DEPTH_CS_HLSli
#define HIZ_DEPTH_CS_HLSli

#include "Common.hlsli"
#include "HiZ.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
// textures[] claims the rest of space0's t-registers, so this sits in its own space.
TYR_VK_BINDING(TYR_BINDING_HIZ, 0) RWStructuredBuffer<float> hizLevels[TYR_BUFFERED_FRAME_COUNT] : register(u0, space2);

TYR_VK_PUSH_CONSTANT HiZPushConstants g_PushConstants;

// Builds Hi-Z level 0 - each texel is the farthest (smallest, with reverse-Z) of its 2x2 depth pixels.
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	const uint2 depthSize = uint2(g_PushConstants.depthWidth, g_PushConstants.depthHeight);
	const uint2 size = HiZLevelSize(depthSize, 0);
	if (dispatchThreadId.x >= size.x || dispatchThreadId.y >= size.y)
	{
		return;
	}

	// Clamped, so an odd-sized edge texel covers its one real column or row twice.
	const uint2 last = depthSize - 1u;
	const uint2 p0 = min(dispatchThreadId.xy * 2u, last);
	const uint2 p1 = min(dispatchThreadId.xy * 2u + 1u, last);

	const uint depthIndex = g_PushConstants.depthIndex;
	const float farthest = min(
		min(textures[depthIndex].Load(int3(p0.x, p0.y, 0)).r, textures[depthIndex].Load(int3(p1.x, p0.y, 0)).r),
		min(textures[depthIndex].Load(int3(p0.x, p1.y, 0)).r, textures[depthIndex].Load(int3(p1.x, p1.y, 0)).r));

	hizLevels[g_PushConstants.renderFrameIndex][dispatchThreadId.y * size.x + dispatchThreadId.x] = farthest;
}

#endif
