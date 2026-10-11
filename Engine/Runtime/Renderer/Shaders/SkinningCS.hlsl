#ifndef SKINNING_CS_HLSli
#define SKINNING_CS_HLSli

#include "Common.hlsli"

TYR_VK_BINDING(TYR_BINDING_VERTEX, 0) RWStructuredBuffer<Vertex> vertices : register(u4);
TYR_VK_BINDING(TYR_BINDING_SKIN_VERTEX, 0) StructuredBuffer<SkinVertex> skinVertices : register(t27);
// Read as rows rather than float4x4, so the matrix layout can't depend on how a bare matrix
// element is packed. Each bone is four rows of a row-vector matrix.
TYR_VK_BINDING(TYR_BINDING_BONE_MATRIX, 0) StructuredBuffer<float4> boneMatrixRows : register(t28);

TYR_VK_PUSH_CONSTANT SkinningPushConstants g_PushConstants;

// One thread per vertex of one skinned mesh instance. Linear blend skinning with up to four
// bones, written over this instance's vertices after keeping last frame's for motion vectors.
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint index = dispatchThreadID.x;
	if (index >= g_PushConstants.vertexCount)
	{
		return;
	}

	const Vertex rest = vertices[g_PushConstants.restVertexOffset + index];
	const SkinVertex skin = skinVertices[g_PushConstants.skinVertexOffset + index];

	// The weighted sum of the bones' matrices, built row by row.
	float4 rows[4] = { (float4)0, (float4)0, (float4)0, (float4)0 };
	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		const uint bone = (skin.boneIndices >> (i * 8)) & 0xFF;
		const float weight = (float)((skin.boneWeights >> (i * 8)) & 0xFF) / 255.0f;
		const uint firstRow = (g_PushConstants.boneMatrixOffset + bone) * 4;
		[unroll]
		for (uint row = 0; row < 4; ++row)
		{
			rows[row] += boneMatrixRows[firstRow + row] * weight;
		}
	}
	const float4x4 skinMatrix = float4x4(rows[0], rows[1], rows[2], rows[3]);

	float bitangentSign;
	const float3 restTangent = DecodeOctTangent(rest.tangentOct, bitangentSign);

	Vertex skinned = rest;
	skinned.position = mul(float4(rest.position, 1.0f), skinMatrix).xyz;
	skinned.normalOct = EncodeOctPacked(normalize(mul(float4(DecodeOct(rest.normalOct), 0.0f), skinMatrix).xyz));
	skinned.tangentOct = EncodeOctTangentPacked(normalize(mul(float4(restTangent, 0.0f), skinMatrix).xyz), bitangentSign);

	const uint output = g_PushConstants.outputVertexOffset + index;
	// The first frame has no last frame, so it uses this one.
	const bool firstFrame = g_PushConstants.firstFrame != 0;
	const Vertex last = vertices[output];
	Vertex previous;
	previous.position = firstFrame ? skinned.position : last.position;
	previous.normalOct = firstFrame ? skinned.normalOct : last.normalOct;
	previous.tangentOct = firstFrame ? skinned.tangentOct : last.tangentOct;
	previous.uv = skinned.uv;
	vertices[g_PushConstants.prevOutputVertexOffset + index] = previous;
	vertices[output] = skinned;
}

#endif
