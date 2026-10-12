#ifndef HIZ_DOWNSAMPLE_CS_HLSli
#define HIZ_DOWNSAMPLE_CS_HLSli

#include "Common.hlsli"
#include "HiZ.hlsli"

TYR_VK_BINDING(TYR_BINDING_HIZ, 0) RWStructuredBuffer<float> hizLevels[TYR_BUFFERED_FRAME_COUNT] : register(u0, space2);

TYR_VK_PUSH_CONSTANT HiZPushConstants g_PushConstants;

// Builds Hi-Z level g_PushConstants.level (1 or more) from the farthest of the 2x2 texels under it in the level before.
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	const uint2 depthSize = uint2(g_PushConstants.depthWidth, g_PushConstants.depthHeight);
	const uint level = g_PushConstants.level;
	const uint2 size = HiZLevelSize(depthSize, level);
	if (dispatchThreadId.x >= size.x || dispatchThreadId.y >= size.y)
	{
		return;
	}

	const uint2 srcSize = HiZLevelSize(depthSize, level - 1u);
	const uint srcOffset = HiZLevelOffset(depthSize, level - 1u);
	const uint dstOffset = srcOffset + srcSize.x * srcSize.y;

	const uint2 last = srcSize - 1u;
	const uint2 p0 = min(dispatchThreadId.xy * 2u, last);
	const uint2 p1 = min(dispatchThreadId.xy * 2u + 1u, last);

	const uint frame = g_PushConstants.renderFrameIndex;
	const float farthest = min(
		min(hizLevels[frame][srcOffset + p0.y * srcSize.x + p0.x], hizLevels[frame][srcOffset + p0.y * srcSize.x + p1.x]),
		min(hizLevels[frame][srcOffset + p1.y * srcSize.x + p0.x], hizLevels[frame][srcOffset + p1.y * srcSize.x + p1.x]));

	hizLevels[frame][dstOffset + dispatchThreadId.y * size.x + dispatchThreadId.x] = farthest;
}

#endif
