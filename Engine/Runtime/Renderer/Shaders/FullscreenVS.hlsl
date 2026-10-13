#ifndef FULLSCREEN_VS_HLSli
#define FULLSCREEN_VS_HLSli

#include "ShaderTypes.h"

struct GUI_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 colour : COLOR0;
};

// One triangle covering the screen, drawn with GUIPS.hlsl to show a viewport's image on the
// whole window when there's no editor UI around it.
GUI_VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	const float2 uv = float2((vertexID << 1) & 2, vertexID & 2);

	GUI_VS_OUTPUT output;
	output.pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
	output.uv = uv;
	output.colour = float4(1.0f, 1.0f, 1.0f, 1.0f);
	return output;
}

#endif
