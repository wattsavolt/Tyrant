#ifndef MESH_VS_HLSli
#define MESH_VS_HLSli

#include "Common.hlsli"

struct VS_IN
{
	float3 pos : POSITION0;
	float3 normal : NORMAL0;
	float4x4 instWorld : WORLD0;
};

cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float3 camPos;
	float ambient;
};

VS_OUTPUT main(VS_IN input)
{
	float4x4 wvp = mul(input.instWorld, ViewProj);

	VS_OUTPUT output = (VS_OUTPUT)0;
	output.pos = mul(float4(input.pos, 1.0f), wvp);
	output.worldPos = mul(float4(input.pos, 1.0f), input.instWorld);

	output.normal = normalize(mul(float4(input.normal, 0.0f), input.instWorld).xyz);

	return output;
}

#endif


