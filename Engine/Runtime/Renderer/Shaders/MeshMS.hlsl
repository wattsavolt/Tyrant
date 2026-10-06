#ifndef MESH_MS_HLSli
#define MESH_MS_HLSli

#include "Common.hlsli"

struct MeshTaskPayload
{
	uint meshInstanceIndex;
	uint globalMeshletIndex;
	uint globalVertexBase;
	uint globalIndexBase;
};

TYR_VK_BINDING(TYR_BINDING_SCENE_INFO, 0)
cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	// Unused here - declared so camPos/ambient below land at the same offset as the real
	// struct this buffer is actually filled from.
	float4x4 InvViewProj;
	float3 camPos;
	float ambient;
};

TYR_VK_BINDING(TYR_BINDING_MESHLET, 0) StructuredBuffer<Meshlet> meshlets : register(t3);
TYR_VK_BINDING(TYR_BINDING_VERTEX, 0) StructuredBuffer<Vertex> vertices : register(t4);
TYR_VK_BINDING(TYR_BINDING_INDEX, 0) StructuredBuffer<uint> indices : register(t5);
TYR_VK_BINDING(TYR_BINDING_MESH_INSTANCE, 0) StructuredBuffer<MeshInstance> meshInstances : register(t6);

[outputtopology("triangle")]
[numthreads(TYR_MAX_MESHLET_TRIANGLES, 1, 1)]
void main(
	uint3 groupThreadId : SV_GroupThreadID,
	in payload MeshTaskPayload payload,
	out indices uint3 tris[TYR_MAX_MESHLET_TRIANGLES],
	out vertices VS_OUTPUT verts[TYR_MAX_MESHLET_VERTICES])
{
	const Meshlet meshlet = meshlets[payload.globalMeshletIndex];
	const uint triangleCount = meshlet.indexCount / 3;

	SetMeshOutputCounts(meshlet.vertexCount, triangleCount);

	const MeshInstance instance = meshInstances[payload.meshInstanceIndex];
	const float4x4 wvp = mul(instance.transform, ViewProj);
	const uint materialIndex = instance.materialIndices[meshlet.materialSlot];

	if (groupThreadId.x < meshlet.vertexCount)
	{
		const Vertex v = vertices[payload.globalVertexBase + meshlet.vertexOffset + groupThreadId.x];

		float bitangentSign;
		const float3 tangentDir = DecodeOctTangent(v.tangentOct, bitangentSign);

		VS_OUTPUT output = (VS_OUTPUT)0;
		output.pos = mul(float4(v.position, 1.0f), wvp);
		output.worldPos = mul(float4(v.position, 1.0f), instance.transform);
		output.normal = normalize(mul(float4(DecodeOct(v.normalOct), 0.0f), instance.transform).xyz);
		output.tangent = float4(normalize(mul(float4(tangentDir, 0.0f), instance.transform).xyz), bitangentSign);
		output.uv = (float2)v.uv;
		output.materialIndex = materialIndex;
		verts[groupThreadId.x] = output;
	}

	if (groupThreadId.x < triangleCount)
	{
		const uint base = payload.globalIndexBase + meshlet.indexOffset + groupThreadId.x * 3;
		tris[groupThreadId.x] = uint3(indices[base + 0], indices[base + 1], indices[base + 2]);
	}
}

#endif
