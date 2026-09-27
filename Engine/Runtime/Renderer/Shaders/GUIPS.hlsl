#ifndef GUI_PS_HLSli
#define GUI_PS_HLSli

#include "ShaderTypes.h"

struct PushConstants
{
	float2 scale;
	float2 translate;
	uint textureIndex;
};
[[vk::push_constant]] PushConstants g_PushConstants;

[[vk::binding(TYR_BINDING_TEXTURES, 0)]] Texture2D textures[] : register(t11);
[[vk::binding(TYR_BINDING_SAMPLERS, 0)]] SamplerState samplers[] : register(s12);

struct GUI_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 colour : COLOR0;
};

float4 main(GUI_VS_OUTPUT input) : SV_TARGET
{
	return input.colour * textures[g_PushConstants.textureIndex].Sample(samplers[0], input.uv);
}

#endif
