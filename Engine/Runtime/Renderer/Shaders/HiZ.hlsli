#ifndef HIZ_HLSli
#define HIZ_HLSli

#include "ShaderTypes.h"

// The Hi-Z pyramid holds the farthest depth under each texel, levels packed one after another in
// one buffer. Level 0 is half the depth buffer's size rounded up, and each level after halves the
// one before, down to 1x1. A texel at level L covers 2^(L+1) depth pixels along each axis.

uint2 HiZLevelSize(uint2 depthSize, uint level)
{
	const uint shift = level + 1;
	return (depthSize + ((1u << shift) - 1u)) >> shift;
}

// Matches Renderer's GetHiZLevelCount.
uint HiZLevelCount(uint2 depthSize)
{
	const uint largest = max(max(depthSize.x, depthSize.y), 2u);
	return firstbithigh(largest - 1u) + 1u;
}

uint HiZLevelOffset(uint2 depthSize, uint level)
{
	uint offset = 0;
	[unroll]
	for (uint i = 0; i < TYR_MAX_HIZ_LEVELS; ++i)
	{
		const uint2 size = HiZLevelSize(depthSize, i);
		offset += size.x * size.y * (uint)(i < level);
	}
	return offset;
}

#endif
