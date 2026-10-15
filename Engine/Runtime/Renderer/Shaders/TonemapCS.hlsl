#ifndef TONEMAP_CS_HLSli
#define TONEMAP_CS_HLSli

#include "Common.hlsli"
#include "ColourUtility.hlsli"

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);
TYR_VK_BINDING(TYR_BINDING_EXPOSURE, 0) StructuredBuffer<ExposureState> exposureStates : register(t5, space1);
TYR_VK_BINDING(TYR_BINDING_DISPLAY_OUTPUT, 0) RWTexture2D<float4> outputImages[TYR_BUFFERED_FRAME_COUNT] : register(u34);

TYR_VK_PUSH_CONSTANT TonemapPushConstants g_PushConstants;

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= g_PushConstants.width || dispatchThreadId.y >= g_PushConstants.height)
	{
		return;
	}

	// The first bloom level holds every level added up, so it's averaged back to the image's brightness.
	const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(g_PushConstants.width, g_PushConstants.height);
	const float2 bloomHalfTexel = 1.0f / float2(g_PushConstants.width, g_PushConstants.height);
	const float3 bloom = textures[g_PushConstants.bloomIndex].SampleLevel(samplers[0], clamp(uv, bloomHalfTexel, 1.0f - bloomHalfTexel), 0.0f).rgb
		* (1.0f / TYR_BLOOM_LEVELS);
	const float3 scene = textures[g_PushConstants.colourIndex].Load(int3(dispatchThreadId.xy, 0)).rgb;
	const float3 colour = lerp(scene, bloom, g_PushConstants.bloomIntensity);
	const float exposure = exposureStates[g_PushConstants.viewportIndex].exposure;
	const float3 mapped = TonemapUchimura(colour * exposure, g_PushConstants.peakBrightness);

	// A flash washes over the image, most at its edges, and fading takes it to black.
	const float2 fromCentre = uv * 2.0f - 1.0f;
	const float edge = saturate(dot(fromCentre, fromCentre) * 0.5f);
	const float flash = saturate(g_PushConstants.flash * (0.3f + 0.7f * edge));
	const float3 flashColour = float3(g_PushConstants.flashR, g_PushConstants.flashG, g_PushConstants.flashB);
	const float3 shown = lerp(mapped, flashColour, flash) * (1.0f - saturate(g_PushConstants.fade));
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(shown * g_PushConstants.outputScale, 1.0f);
}

#endif
