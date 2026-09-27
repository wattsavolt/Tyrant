#ifndef GUI_VS_HLSli
#define GUI_VS_HLSli

#include "ShaderTypes.h"

// Matches GUIVertex in Rendering/GUIDrawData.h byte for byte.
struct GUIVertex
{
	float2 pos;
	float2 uv;
	uint colour;
};

struct PushConstants
{
	float2 scale;
	float2 translate;
	uint textureIndex;
};
[[vk::push_constant]] PushConstants g_PushConstants;

[[vk::binding(TYR_BINDING_GUI_VERTEX, 0)]] StructuredBuffer<GUIVertex> vertices : register(t13);

struct GUI_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 colour : COLOR0;
};

// Unpacks a colour stored as 4 bytes (R in the lowest byte, matching GUIVertex::colour's own
// packing) into a 0-1 float4.
float4 UnpackColour(uint packed)
{
	return float4(
		(packed & 0xFF) / 255.0f,
		((packed >> 8) & 0xFF) / 255.0f,
		((packed >> 16) & 0xFF) / 255.0f,
		((packed >> 24) & 0xFF) / 255.0f);
}

GUI_VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	// Reads its own vertex straight out of a StructuredBuffer using SV_VertexID instead of
	// fixed-function vertex input - the index buffer is still bound normally, so SV_VertexID is
	// already the resolved vertex index after that lookup.
	const GUIVertex v = vertices[vertexID];

	GUI_VS_OUTPUT output;
	output.pos = float4(v.pos * g_PushConstants.scale + g_PushConstants.translate, 0.0f, 1.0f);
	output.uv = v.uv;
	output.colour = UnpackColour(v.colour);
	return output;
}

#endif
