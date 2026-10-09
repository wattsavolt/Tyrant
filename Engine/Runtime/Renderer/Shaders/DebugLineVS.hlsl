#ifndef DEBUG_LINE_VS_HLSli
#define DEBUG_LINE_VS_HLSli

#include "ShaderTypes.h"

// Matches DebugLineVertex in Rendering/DebugLineTypes.h byte for byte.
struct DebugLineVertex
{
	float3 position;
	uint colour;
};

// Matches DebugLinePushConstants in Renderer.cpp byte for byte.
struct PushConstants
{
	// Without TAA jitter, since lines are drawn after TAA.
	float4x4 viewProj;
	uint firstVertex;
};
TYR_VK_PUSH_CONSTANT PushConstants g_PushConstants;

TYR_VK_BINDING(TYR_BINDING_DEBUG_LINE_VERTEX, 0) StructuredBuffer<DebugLineVertex> vertices : register(t26);

// Moves lines this fraction of their distance towards the camera, so lines lying on a surface
// aren't hidden by it. Reverse-Z, so a larger depth is nearer.
static const float c_DepthBias = 0.002f;

struct DEBUG_LINE_VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 colour : COLOR0;
};

float4 UnpackColour(uint packed)
{
	return float4(
		(packed & 0xFF) / 255.0f,
		((packed >> 8) & 0xFF) / 255.0f,
		((packed >> 16) & 0xFF) / 255.0f,
		((packed >> 24) & 0xFF) / 255.0f);
}

DEBUG_LINE_VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	const DebugLineVertex v = vertices[g_PushConstants.firstVertex + vertexID];

	DEBUG_LINE_VS_OUTPUT output;
	output.pos = mul(float4(v.position, 1.0f), g_PushConstants.viewProj);
	output.pos.z *= 1.0f + c_DepthBias;
	output.colour = UnpackColour(v.colour);
	return output;
}

#endif
