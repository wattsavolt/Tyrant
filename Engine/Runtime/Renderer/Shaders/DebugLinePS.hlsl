#ifndef DEBUG_LINE_PS_HLSli
#define DEBUG_LINE_PS_HLSli

#include "ShaderTypes.h"

struct DEBUG_LINE_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 colour : COLOR0;
};

float4 main(DEBUG_LINE_VS_OUTPUT input) : SV_TARGET
{
	return input.colour;
}

#endif
