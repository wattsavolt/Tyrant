#ifndef GUI_PS_HLSli
#define GUI_PS_HLSli

#include "ShaderTypes.h"
#include "ColourUtility.hlsli"

// 1 to write HDR10 for an HDR swap chain, otherwise linear colour that an sRGB swap chain encodes.
#ifndef TYR_OUTPUT_HDR10
#define TYR_OUTPUT_HDR10 0
#endif

struct PushConstants
{
	float2 scale;
	float2 translate;
	uint textureIndex;
	// How bright white is, where 1 is 10000 nits. Only HDR10 output uses it.
	float brightness;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

TYR_VK_BINDING(TYR_BINDING_TEXTURES, 0) Texture2D textures[] : register(t11);
TYR_VK_BINDING(TYR_BINDING_SAMPLERS, 0) SamplerState samplers[] : register(s12);

struct GUI_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 colour : COLOR0;
};

float4 main(GUI_VS_OUTPUT input) : SV_TARGET
{
	// UI colours are sRGB. Textures are already linear, or made linear by their sRGB format.
	const float4 colour = float4(SRGBToLinear(input.colour.rgb), input.colour.a) * textures[g_PushConstants.textureIndex].Sample(samplers[0], input.uv);

	// Dithered where the output rounds, so dark gradients don't band.
	const float noise = InterleavedGradientNoise(input.pos.xy) - 0.5f;
#if TYR_OUTPUT_HDR10
	return float4(EncodePQ(Rec709ToRec2020(colour.rgb) * g_PushConstants.brightness) + noise / 1023.0f, colour.a);
#else
	return float4(SRGBToLinear(saturate(LinearToSRGB(colour.rgb) + noise / 255.0f)), colour.a);
#endif
}

#endif
