#ifndef BLOOM_DOWNSAMPLE_CS_HLSli
#define BLOOM_DOWNSAMPLE_CS_HLSli

#include "Common.hlsli"
#include "ColourUtility.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);
TYR_VK_BINDING(TYR_BINDING_BLOOM, 0) TYR_VK_IMAGE_FORMAT("rgba16f") RWTexture2D<float4> bloomImages[TYR_BUFFERED_FRAME_COUNT * TYR_BLOOM_LEVELS] : register(u35);

TYR_VK_PUSH_CONSTANT BloomPushConstants g_PushConstants;

// Bilinear sample of the source, kept half a texel in from its edges so it never wraps.
float3 SampleSource(float2 uv)
{
	const float2 halfTexel = 0.5f / float2(g_PushConstants.sourceWidth, g_PushConstants.sourceHeight);
	const float2 clamped = clamp(uv, halfTexel, 1.0f - halfTexel);
	return textures[g_PushConstants.sourceIndex].SampleLevel(samplers[0], clamped, 0.0f).rgb;
}

// Weighs a group of samples by its average, or down by its brightness for the first level.
float GroupWeight(float3 average, float baseWeight)
{
	const float karis = 1.0f / (1.0f + Luminance(average));
	return baseWeight * lerp(1.0f, karis, (float)g_PushConstants.karisAverage);
}

// Halves the source with 13 samples in five overlapping groups (Jimenez 2014).
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 texel = 1.0f / float2(g_PushConstants.sourceWidth, g_PushConstants.sourceHeight);

	const float3 a = SampleSource(uv + texel * float2(-2.0f, -2.0f));
	const float3 b = SampleSource(uv + texel * float2(0.0f, -2.0f));
	const float3 c = SampleSource(uv + texel * float2(2.0f, -2.0f));
	const float3 d = SampleSource(uv + texel * float2(-2.0f, 0.0f));
	const float3 e = SampleSource(uv);
	const float3 f = SampleSource(uv + texel * float2(2.0f, 0.0f));
	const float3 g = SampleSource(uv + texel * float2(-2.0f, 2.0f));
	const float3 h = SampleSource(uv + texel * float2(0.0f, 2.0f));
	const float3 i = SampleSource(uv + texel * float2(2.0f, 2.0f));
	const float3 j = SampleSource(uv + texel * float2(-1.0f, -1.0f));
	const float3 k = SampleSource(uv + texel * float2(1.0f, -1.0f));
	const float3 l = SampleSource(uv + texel * float2(-1.0f, 1.0f));
	const float3 m = SampleSource(uv + texel * float2(1.0f, 1.0f));

	const float3 centre = (j + k + l + m) * 0.25f;
	const float3 topLeft = (a + b + d + e) * 0.25f;
	const float3 topRight = (b + c + e + f) * 0.25f;
	const float3 bottomLeft = (d + e + g + h) * 0.25f;
	const float3 bottomRight = (e + f + h + i) * 0.25f;

	const float centreWeight = GroupWeight(centre, 0.5f);
	const float topLeftWeight = GroupWeight(topLeft, 0.125f);
	const float topRightWeight = GroupWeight(topRight, 0.125f);
	const float bottomLeftWeight = GroupWeight(bottomLeft, 0.125f);
	const float bottomRightWeight = GroupWeight(bottomRight, 0.125f);

	const float3 sum = centre * centreWeight + topLeft * topLeftWeight + topRight * topRightWeight
		+ bottomLeft * bottomLeftWeight + bottomRight * bottomRightWeight;
	const float weight = centreWeight + topLeftWeight + topRightWeight + bottomLeftWeight + bottomRightWeight;

	// Kept within what a half float holds, so one bad pixel can't spread infinity.
	const float3 colour = clamp(sum / max(weight, 0.0001f), 0.0f, 60000.0f);
	bloomImages[g_PushConstants.renderFrameIndex * TYR_BLOOM_LEVELS + g_PushConstants.level][dispatchThreadId.xy] = float4(colour, 1.0f);
}

#endif
