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

TYR_VK_BINDING(TYR_BINDING_MESH, 0) StructuredBuffer<Mesh> meshes : register(t1);
TYR_VK_BINDING(TYR_BINDING_MESH_LOD, 0) StructuredBuffer<MeshLOD> meshLODs : register(t2);
TYR_VK_BINDING(TYR_BINDING_MESH_INSTANCE, 0) StructuredBuffer<MeshInstance> meshInstances : register(t6);
// Written by a compute pre-pass - indexed by this draw's slot (via the DrawIndex builtin
// below) rather than a push constant, since a single indirect multi-draw call can't vary
// push constants per sub-draw.
TYR_VK_BINDING(TYR_BINDING_VISIBLE_INSTANCE_INDICES, 0) StructuredBuffer<uint> visibleInstanceIndices : register(t16);

groupshared MeshTaskPayload s_Payload;

// One task shader group per meshlet - this draw's indirect group count already equals this
// instance's meshlet count, so every group unconditionally emits its own meshlet. Frustum
// culling already happened before this shader runs.
#ifndef TYR_VULKAN
// D3D12 has no DrawIndex builtin, so the indirect command signature sets this per draw instead.
cbuffer DrawConstants : register(b0, space1)
{
	uint g_DrawIndex;
};
#endif

[numthreads(1, 1, 1)]
#ifdef TYR_VULKAN
void main(uint3 groupId : SV_GroupID, [[vk::builtin("DrawIndex")]] uint drawIndex : DRAWINDEX)
#else
void main(uint3 groupId : SV_GroupID)
#endif
{
#ifndef TYR_VULKAN
	const uint drawIndex = g_DrawIndex;
#endif
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
