#ifndef MESH_AS_HLSli
#define MESH_AS_HLSli

#include "Common.hlsli"

struct MeshTaskPayload
{
	uint meshInstanceIndex;
	uint globalMeshletIndex;
	uint globalVertexBase;
	uint globalIndexBase;
};

[[vk::binding(TYR_BINDING_MESH, 0)]] StructuredBuffer<Mesh> meshes : register(t1);
[[vk::binding(TYR_BINDING_MESH_LOD, 0)]] StructuredBuffer<MeshLOD> meshLODs : register(t2);
[[vk::binding(TYR_BINDING_MESH_INSTANCE, 0)]] StructuredBuffer<MeshInstance> meshInstances : register(t6);
// Written by a compute pre-pass - indexed by this draw's slot (via the DrawIndex builtin
// below) rather than a push constant, since a single indirect multi-draw call can't vary
// push constants per sub-draw.
[[vk::binding(TYR_BINDING_VISIBLE_INSTANCE_INDICES, 0)]] StructuredBuffer<uint> visibleInstanceIndices : register(t16);

groupshared MeshTaskPayload s_Payload;

// One task shader group per meshlet - this draw's indirect group count already equals this
// instance's meshlet count, so every group unconditionally emits its own meshlet. Frustum
// culling already happened before this shader runs.
[numthreads(1, 1, 1)]
void main(uint3 groupId : SV_GroupID, [[vk::builtin("DrawIndex")]] uint drawIndex : DRAWINDEX)
{
	const uint meshInstanceIndex = visibleInstanceIndices[drawIndex];
	const MeshInstance instance = meshInstances[meshInstanceIndex];
	const Mesh mesh = meshes[instance.meshIndex];
	// LOD selection isn't implemented yet - always use LOD 0.
	const MeshLOD lod = meshLODs[mesh.lodOffset];

	s_Payload.meshInstanceIndex = meshInstanceIndex;
	s_Payload.globalMeshletIndex = lod.meshletOffset + groupId.x;
	s_Payload.globalVertexBase = lod.vertexOffset;
	s_Payload.globalIndexBase = lod.indexOffset;

	DispatchMesh(1, 1, 1, s_Payload);
}

#endif
