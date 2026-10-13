#ifndef EXPOSURE_CS_HLSli
#define EXPOSURE_CS_HLSli

#include "Common.hlsli"

TYR_VK_BINDING(TYR_BINDING_EXPOSURE, 0) RWStructuredBuffer<ExposureState> exposureStates : register(u5, space1);

TYR_VK_PUSH_CONSTANT ExposurePushConstants g_PushConstants;

static const uint c_BinCount = TYR_EXPOSURE_HISTOGRAM_BIN_COUNT;

groupshared float gs_Prefix[c_BinCount];
groupshared float gs_Weighted[c_BinCount];
groupshared float gs_Counted[c_BinCount];

// One group, one thread per bin. Averages the log luminance of the pixels between the low and
// high percentiles, then moves the exposure towards what makes that average middle grey.
[numthreads(c_BinCount, 1, 1)]
void main(uint groupIndex : SV_GroupIndex)
{
	const uint viewportIndex = g_PushConstants.viewportIndex;
	const float count = (float)exposureStates[viewportIndex].histogram[groupIndex];

	// Each bin's running total, darkest first.
	gs_Prefix[groupIndex] = count;
	GroupMemoryBarrierWithGroupSync();
	for (uint offset = 1; offset < c_BinCount; offset <<= 1)
	{
		const float add = gs_Prefix[groupIndex - min(offset, groupIndex)] * (float)(groupIndex >= offset);
		GroupMemoryBarrierWithGroupSync();
		gs_Prefix[groupIndex] += add;
		GroupMemoryBarrierWithGroupSync();
	}

	const float total = gs_Prefix[c_BinCount - 1];
	const float lowCount = total * g_PushConstants.lowPercent;
	const float highCount = total * g_PushConstants.highPercent;
	const float binEnd = gs_Prefix[groupIndex];
	const float binStart = binEnd - count;
	const float kept = max(min(binEnd, highCount) - max(binStart, lowCount), 0.0f);

	// The middle of the bin's range, or the bottom of the range for black.
	const float binPosition = max((float)groupIndex - 0.5f, 0.0f) / (float)(c_BinCount - 2);
	const float logLuminance = g_PushConstants.minLogLuminance + binPosition * g_PushConstants.logLuminanceRange;
	gs_Weighted[groupIndex] = kept * logLuminance;
	gs_Counted[groupIndex] = kept;
	GroupMemoryBarrierWithGroupSync();

	for (uint stride = c_BinCount / 2; stride > 0; stride >>= 1)
	{
		if (groupIndex < stride)
		{
			gs_Weighted[groupIndex] += gs_Weighted[groupIndex + stride];
			gs_Counted[groupIndex] += gs_Counted[groupIndex + stride];
		}
		GroupMemoryBarrierWithGroupSync();
	}

	if (groupIndex == 0)
	{
		const float averageLog = gs_Weighted[0] / max(gs_Counted[0], 1.0f);
		// The EV100 that maps the average luminance to middle grey (Lagarde and de Rousiers 2014).
		const float autoEV100 = clamp(averageLog + log2(100.0f / 12.5f), g_PushConstants.minEV100, g_PushConstants.maxEV100);
		const float targetEV100 = lerp(g_PushConstants.manualEV100, autoEV100, g_PushConstants.autoExposure) - g_PushConstants.compensation;

		// A higher EV100 is darker, so moving up is darkening.
		const float previousEV100 = exposureStates[viewportIndex].ev100;
		const float adapt = lerp(g_PushConstants.adaptBrighten, g_PushConstants.adaptDarken, (float)(targetEV100 > previousEV100));
		const float moved = lerp(previousEV100, targetEV100, adapt);
		// Manual exposure and the first frame go straight to the target.
		const float snap = (float)(g_PushConstants.hasHistory == 0 || g_PushConstants.autoExposure < 0.5f || !isfinite(previousEV100));
		const float ev100 = lerp(moved, targetEV100, snap);

		exposureStates[viewportIndex].ev100 = ev100;
		exposureStates[viewportIndex].exposure = 1.0f / (1.2f * exp2(ev100));
	}
}

#endif
