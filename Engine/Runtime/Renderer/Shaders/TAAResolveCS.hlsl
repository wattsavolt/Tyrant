#ifndef TAA_RESOLVE_CS_HLSli
#define TAA_RESOLVE_CS_HLSli

#include "Common.hlsli"
#include "ColourUtility.hlsli"

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
	float4 _frustumPlanes[6];
	float4x4 PrevViewProj;
	float2 JitterDelta;
};

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_EXPOSURE, 0) StructuredBuffer<ExposureState> exposureStates : register(t5, space1);
TYR_VK_BINDING(TYR_BINDING_TAA_RESOLVE_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u23);

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
	uint depthIndex;
	uint viewportIndex;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

// Blending in this space keeps a few very bright pixels from flickering as they come and go
// (Karis 2014). Exposed first, so how bright counts as very bright follows what's on screen.
float3 Compress(float3 colour, float exposure)
{
	return colour / (1.0f + max(colour.r, max(colour.g, colour.b)) * exposure);
}

float3 Expand(float3 colour, float exposure)
{
	return colour / max(1.0f - max(colour.r, max(colour.g, colour.b)) * exposure, 1e-4f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const int3 pixelCoord = int3(dispatchThreadId.xy, 0);
	const float exposure = exposureStates[g_PushConstants.viewportIndex].exposure;
	const float3 current = Compress(textures[g_PushConstants.colourIndex].Load(pixelCoord).rgb, exposure);

	if (!g_PushConstants.hasHistory)
	{
		outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(Expand(current, exposure), 1.0f);
		return;
	}

	// History is clamped to the colours around this pixel now, so a bad reprojection, such as
	// something moving or coming into view, can't leave a ghost.
	float3 neighbourMin = current;
	float3 neighbourMax = current;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			const int2 sampleCoord = clamp(int2(dispatchThreadId.xy) + int2(x, y),
				int2(0, 0), int2(g_PushConstants.width - 1, g_PushConstants.height - 1));
			const float3 neighbour = Compress(textures[g_PushConstants.colourIndex].Load(int3(sampleCoord, 0)).rgb, exposure);
			neighbourMin = min(neighbourMin, neighbour);
			neighbourMax = max(neighbourMax, neighbour);
		}
	}

	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);

	// The sky has no motion of its own in the G-buffer, so its motion comes from how the camera turned.
	const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	const float4 farPoint = mul(float4(ndc, 0.0f, 1.0f), InvViewProj);
	const float4 previousClip = mul(float4(farPoint.xyz / farPoint.w, 1.0f), PrevViewProj);
	const float2 skyMotion = ndc - previousClip.xy / previousClip.w - JitterDelta;
	const float isSky = (float)(textures[g_PushConstants.depthIndex].Load(pixelCoord).r <= 0.0f);
	const float2 motion = lerp(textures[g_PushConstants.motionIndex].Load(pixelCoord).rg, skyMotion, isSky);

	// Motion is in NDC, which runs the other way to rows vertically.
	const float2 prevUv = uv - motion * float2(0.5f, -0.5f);

	// No history for a pixel that was off-screen last frame.
	const float onScreen = (float)all(prevUv == saturate(prevUv));
	const int2 maxCoord = int2(g_PushConstants.width - 1, g_PushConstants.height - 1);
	const int2 prevPixelCoord = min(int2(saturate(prevUv) * float2(g_PushConstants.width, g_PushConstants.height)), maxCoord);
	const float3 history = Compress(textures[g_PushConstants.prevResolvedIndex].Load(int3(prevPixelCoord, 0)).rgb, exposure);
	const float3 clampedHistory = clamp(history, neighbourMin, neighbourMax);
	const float3 result = lerp(current, clampedHistory, g_PushConstants.historyBlendWeight * onScreen);

	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(Expand(result, exposure), 1.0f);
}

#endif
