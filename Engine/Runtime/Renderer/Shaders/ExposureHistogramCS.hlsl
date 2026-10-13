#ifndef EXPOSURE_HISTOGRAM_CS_HLSli
#define EXPOSURE_HISTOGRAM_CS_HLSli

#include "Common.hlsli"
#include "ColourUtility.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_EXPOSURE, 0) RWStructuredBuffer<ExposureState> exposureStates : register(u5, space1);

TYR_VK_PUSH_CONSTANT ExposureHistogramPushConstants g_PushConstants;

static const uint c_GroupSize = 16;
// Each thread counts a 2x2 block, so a group covers 32x32 pixels.
static const uint c_PixelsPerThread = 2;

groupshared uint gs_Histogram[TYR_EXPOSURE_HISTOGRAM_BIN_COUNT];

uint GetBin(float3 colour)
{
	const float luminance = Luminance(colour);
	const float position = saturate((log2(max(luminance, 1e-10f)) - g_PushConstants.minLogLuminance) * g_PushConstants.invLogLuminanceRange);
	// Bin 0 holds black, so the other bins only hold what's lit.
	const uint litBin = (uint)(position * (TYR_EXPOSURE_HISTOGRAM_BIN_COUNT - 2)) + 1;
	return litBin * (uint)(luminance > 1e-6f);
}

[numthreads(c_GroupSize, c_GroupSize, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	gs_Histogram[groupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	const uint2 size = uint2(g_PushConstants.width, g_PushConstants.height);
	for (uint i = 0; i < c_PixelsPerThread * c_PixelsPerThread; ++i)
	{
		const uint2 pixel = dispatchThreadId.xy * c_PixelsPerThread + uint2(i % c_PixelsPerThread, i / c_PixelsPerThread);
		const float3 colour = textures[g_PushConstants.colourIndex].Load(int3(min(pixel, size - 1), 0)).rgb;

		// What's in the middle of the view counts up to four times as much as the edges.
		const float2 fromCentre = (float2(pixel) + 0.5f) / float2(size) * 2.0f - 1.0f;
		const uint weight = 1 + (uint)(3.0f * saturate(1.0f - length(fromCentre)));
		const uint inside = (uint)all(pixel < size);
		InterlockedAdd(gs_Histogram[GetBin(colour)], weight * inside);
	}
	GroupMemoryBarrierWithGroupSync();

	const uint count = gs_Histogram[groupIndex];
	if (count > 0)
	{
		InterlockedAdd(exposureStates[g_PushConstants.viewportIndex].histogram[groupIndex], count);
	}
}

#endif
