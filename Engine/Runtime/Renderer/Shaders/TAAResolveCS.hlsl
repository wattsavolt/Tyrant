#ifndef TAA_RESOLVE_CS_HLSli
#define TAA_RESOLVE_CS_HLSli

#include "Common.hlsli"

[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
[[vk::binding(TYR_BINDING_TAA_RESOLVE_OUTPUT, 0)]] RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u23);

// Matches TAAResolvePushConstants in Renderer.cpp byte for byte.
struct PushConstants
{
	uint colourIndex;
	uint motionIndex;
	uint prevResolvedIndex;
	uint width;
	uint height;
	uint renderFrameIndex;
	uint hasHistory;
	float historyBlendWeight;
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
	const float3 current = textures[g_PushConstants.colourIndex].Load(pixelCoord).rgb;

	if (!g_PushConstants.hasHistory)
	{
		outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(current, 1.0f);
		return;
	}

	// This tick's neighbourhood colour bounds, for clamping the reprojected history sample
	// below - this is what keeps a bad reprojection (anything the camera-only motion vectors
	// don't account for, like a moving object or a disocclusion) from showing up as a ghosting
	// trail: history is never allowed to pull the result outside what this tick's own
	// neighbourhood actually contains.
	float3 neighbourMin = current;
	float3 neighbourMax = current;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			if (x == 0 && y == 0)
			{
				continue;
			}
			const int2 sampleCoord = clamp(int2(dispatchThreadId.xy) + int2(x, y),
				int2(0, 0), int2(g_PushConstants.width - 1, g_PushConstants.height - 1));
			const float3 neighbour = textures[g_PushConstants.colourIndex].Load(int3(sampleCoord, 0)).rgb;
			neighbourMin = min(neighbourMin, neighbour);
			neighbourMax = max(neighbourMax, neighbour);
		}
	}

	// Reproject this pixel into where it was last frame - motion is already de-jittered (see
	// GBufferPS.hlsl), so this is real scene motion only.
	const float2 motion = textures[g_PushConstants.motionIndex].Load(pixelCoord).rg;
	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 prevUv = uv - motion * 0.5f;

	float3 result = current;
	if (prevUv.x >= 0.0f && prevUv.x <= 1.0f && prevUv.y >= 0.0f && prevUv.y <= 1.0f)
	{
		const int2 prevPixelCoord = int2(prevUv * float2(g_PushConstants.width, g_PushConstants.height));
		const float3 history = textures[g_PushConstants.prevResolvedIndex].Load(int3(prevPixelCoord, 0)).rgb;
		const float3 clampedHistory = clamp(history, neighbourMin, neighbourMax);
		result = lerp(current, clampedHistory, g_PushConstants.historyBlendWeight);
	}

	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(result, 1.0f);
}

#endif
