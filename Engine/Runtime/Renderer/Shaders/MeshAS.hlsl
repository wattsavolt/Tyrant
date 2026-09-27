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

struct PushConstants
{
	uint meshInstanceIndex;
};
[[vk::push_constant]] PushConstants g_PushConstants;

[[vk::binding(TYR_BINDING_MESH, 0)]] StructuredBuffer<Mesh> meshes : register(t1);
[[vk::binding(TYR_BINDING_MESH_LOD, 0)]] StructuredBuffer<MeshLOD> meshLODs : register(t2);
[[vk::binding(TYR_BINDING_MESH_INSTANCE, 0)]] StructuredBuffer<MeshInstance> meshInstances : register(t6);

groupshared MeshTaskPayload s_Payload;

// One task shader group per meshlet - the group count dispatched from the CPU is exactly
// this instance's meshlet count, so every group unconditionally emits its own meshlet. GPU
// culling (frustum/occlusion, deciding which groups emit a mesh shader group at all) is the
// next step to add here.
[numthreads(1, 1, 1)]
void main(uint3 groupId : SV_GroupID)
{
	const MeshInstance instance = meshInstances[g_PushConstants.meshInstanceIndex];
	const Mesh mesh = meshes[instance.meshIndex];
	// LOD selection isn't implemented yet - always use LOD 0.
	const MeshLOD lod = meshLODs[mesh.lodOffset];

	s_Payload.meshInstanceIndex = g_PushConstants.meshInstanceIndex;
	s_Payload.globalMeshletIndex = lod.meshletOffset + groupId.x;
	s_Payload.globalVertexBase = lod.vertexOffset;
	s_Payload.globalIndexBase = lod.indexOffset;

	DispatchMesh(1, 1, 1, s_Payload);
}

#endif
