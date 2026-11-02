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
	// Wounds darken the edges to blood red, reaching further in the worse they are.
	const float wounds = saturate(g_PushConstants.wounds);
	const float2 squashed = fromCentre * float2(1.0f, 0.8f);
	const float woundEdge = smoothstep(1.25f - wounds * 0.75f, 1.45f - wounds * 0.45f, length(squashed)) * (0.35f + 0.6f * wounds);
	const float3 wounded = lerp(mapped, float3(0.22f, 0.0f, 0.0f), woundEdge * step(0.001f, wounds));
	// Blood splashes out from the middle in a ragged blot, with drops thrown further, as it builds up.
	const float blood = saturate(g_PushConstants.blood);
	const float2 splash = fromCentre * float2(float(g_PushConstants.width) / float(g_PushConstants.height), 1.0f);
	const float angle = atan2(splash.y, splash.x);
	const float ragged = 0.06f * sin(angle * 7.0f + 1.3f) + 0.04f * sin(angle * 13.0f + 4.1f) + 0.025f * sin(angle * 29.0f + 2.2f);
	const float reach = (0.12f + 0.75f * blood) * (1.0f + ragged * 2.0f);
	float blot = 1.0f - smoothstep(reach - 0.04f, reach, length(splash));
	const float2 drops[6] = { float2(0.52f, 0.31f), float2(-0.61f, 0.12f), float2(0.18f, -0.58f), float2(-0.35f, -0.44f), float2(0.71f, -0.22f), float2(-0.2f, 0.63f) };
	[unroll]
	for (uint i = 0; i < 6; i++)
	{
		const float2 at = drops[i] * (0.4f + 0.9f * blood);
		blot = max(blot, (1.0f - smoothstep(0.02f + 0.05f * blood, 0.03f + 0.06f * blood, length(splash - at))) * step(0.15f * float(i), blood));
	}
	// Dark, nearly black red, thickest towards the middle of the blot.
	const float3 bloodColour = lerp(float3(0.11f, 0.008f, 0.006f), float3(0.035f, 0.0f, 0.0f), saturate(1.0f - length(splash) / max(reach, 0.01f)));
	const float3 bloodied = lerp(wounded, bloodColour, blot * saturate(blood * 4.0f) * 0.95f);
	const float3 shown = lerp(bloodied, flashColour, flash) * (1.0f - saturate(g_PushConstants.fade));
	outputImages[g_PushConstants.renderFrameIndex][dispatchThreadId.xy] = float4(shown * g_PushConstants.outputScale, 1.0f);
}

#endif
