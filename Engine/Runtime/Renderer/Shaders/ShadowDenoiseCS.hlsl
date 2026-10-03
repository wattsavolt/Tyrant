#ifndef SHADOW_DENOISE_CS_HLSli
#define SHADOW_DENOISE_CS_HLSli

#include "Common.hlsli"

[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
// shadowMasksRaw is a 3-element array starting at u20, so it occupies u20-u22 in HLSL's register
// model - shadowMasks needs to start past that (u23) to avoid a bogus overlap. As with tlas in
// ShadowRTCS.hlsl, the real Vulkan binding slot comes entirely from [[vk::binding]]; this
// register() annotation just needs to be unique within this file.
[[vk::binding(TYR_BINDING_SHADOW_MASKS_RAW, 0)]] RWTexture2DArray<float2> shadowMasksRaw[TYR_BUFFERED_FRAME_COUNT] : register(u20);
[[vk::binding(TYR_BINDING_SHADOW_MASKS, 0)]] RWTexture2DArray<float2> shadowMasks[TYR_BUFFERED_FRAME_COUNT] : register(u23);

// Matches ShadowDenoisePushConstants in ShadowDenoisePass.h byte for byte.
struct PushConstants
{
	uint depthIndex;
	uint motionIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
	uint prevRenderFrameIndex;
	uint slot;
	uint hasHistory;
	uint spatialRadius;
};
[[vk::push_constant]] PushConstants g_PushConstants;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
	const uint3 outputCoord = uint3(dispatchThreadId.xy, g_PushConstants.slot);

	const float centreDepth = textures[g_PushConstants.depthIndex].Load(pixelCoord).r;
	// Nothing was drawn here - same background convention ShadowRTCS.hlsl uses.
	if (centreDepth <= 0.0f)
	{
		shadowMasks[g_PushConstants.renderFrameIndex][outputCoord] = float2(1.0f, 0.0f);
		return;
	}

	// Depth-aware spatial blur of this tick's raw trace - skips neighbours whose depth differs
	// too much, since those are likely a different surface entirely and blurring across them
	// would leak light/shadow between unrelated objects.
	float spatialSum = 0.0f;
	float spatialWeight = 0.0f;
	const int radius = (int)g_PushConstants.spatialRadius;
	for (int y = -radius; y <= radius; ++y)
	{
		for (int x = -radius; x <= radius; ++x)
		{
			const int2 sampleCoord = int2(dispatchThreadId.xy) + int2(x, y);
			if (sampleCoord.x < 0 || sampleCoord.y < 0 ||
				sampleCoord.x >= (int)g_PushConstants.width || sampleCoord.y >= (int)g_PushConstants.height)
			{
				continue;
			}

			const float sampleDepth = textures[g_PushConstants.depthIndex].Load(int3(sampleCoord, 0)).r;
			if (sampleDepth <= 0.0f)
			{
				continue;
			}

			// Reverse-Z - a relative difference rather than an absolute one, so this works at
			// both near and far depths without needing a scene-scale-dependent threshold.
			const float depthDiff = abs(sampleDepth - centreDepth) / max(centreDepth, 0.0001f);
			if (depthDiff > 0.05f)
			{
				continue;
			}

			const float raw = shadowMasksRaw[g_PushConstants.renderFrameIndex][uint3(sampleCoord, g_PushConstants.slot)].r;
			spatialSum += raw;
			spatialWeight += 1.0f;
		}
	}
	const float rawCentre = shadowMasksRaw[g_PushConstants.renderFrameIndex][uint3(dispatchThreadId.xy, g_PushConstants.slot)].r;
	const float spatialResult = spatialWeight > 0.0f ? (spatialSum / spatialWeight) : rawCentre;

	float finalResult = spatialResult;
	if (g_PushConstants.hasHistory)
	{
		// gbufferMotion is current-NDC minus previous-NDC (see GBufferPS.hlsl) - subtracting it
		// (halved, to go from NDC's [-1,1] range to UV's [0,1] range) reprojects this pixel back
		// into where it was last frame.
		const float2 motion = textures[g_PushConstants.motionIndex].Load(pixelCoord).rg;
		const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
		const float2 prevUv = uv - motion * 0.5f;

		if (prevUv.x >= 0.0f && prevUv.x <= 1.0f && prevUv.y >= 0.0f && prevUv.y <= 1.0f)
		{
			const int2 prevPixelCoord = int2(prevUv * float2(g_PushConstants.width, g_PushConstants.height));
			const float history = shadowMasks[g_PushConstants.prevRenderFrameIndex][uint3(prevPixelCoord, g_PushConstants.slot)].r;
			// Exponential moving average - no disocclusion rejection beyond the on-screen check
			// above. A bad blend self-corrects within a few frames as new raw samples keep
			// arriving, the same trade-off already accepted for this feature's slot-reassignment
			// cold-start case.
			finalResult = lerp(spatialResult, history, 0.85f);
		}
	}

	shadowMasks[g_PushConstants.renderFrameIndex][outputCoord] = float2(finalResult, 0.0f);
}

#endif
