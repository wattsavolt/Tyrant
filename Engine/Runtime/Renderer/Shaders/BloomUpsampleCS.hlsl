#ifndef BLOOM_UPSAMPLE_CS_HLSli
#define BLOOM_UPSAMPLE_CS_HLSli

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

// Adds the smaller level above, blurred with a 3x3 tent, onto this level.
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 texel = 1.0f / float2(g_PushConstants.sourceWidth, g_PushConstants.sourceHeight);

	float3 blurred = SampleSource(uv) * 4.0f;
	blurred += (SampleSource(uv + texel * float2(0.0f, -1.0f)) + SampleSource(uv + texel * float2(-1.0f, 0.0f))
		+ SampleSource(uv + texel * float2(1.0f, 0.0f)) + SampleSource(uv + texel * float2(0.0f, 1.0f))) * 2.0f;
	blurred += SampleSource(uv + texel * float2(-1.0f, -1.0f)) + SampleSource(uv + texel * float2(1.0f, -1.0f))
		+ SampleSource(uv + texel * float2(-1.0f, 1.0f)) + SampleSource(uv + texel * float2(1.0f, 1.0f));
	blurred *= 1.0f / 16.0f;

	const uint image = g_PushConstants.renderFrameIndex * TYR_BLOOM_LEVELS + g_PushConstants.level;
	const float3 colour = bloomImages[image][dispatchThreadId.xy].rgb + blurred;
	bloomImages[image][dispatchThreadId.xy] = float4(min(colour, 60000.0f), 1.0f);
}

#endif
