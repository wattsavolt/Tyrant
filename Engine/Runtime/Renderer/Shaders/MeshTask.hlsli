#ifndef MESH_TASK_HLSli
#define MESH_TASK_HLSli

// Culls one draw's meshlets, TYR_MESHLETS_PER_TASK_GROUP per group, and launches a mesh shader
// group for each survivor. The late pass (TYR_LATE_CULL 1) also tests them against the Hi-Z.

#include "Culling.hlsli"
#include "MeshTaskPayload.hlsli"

TYR_VK_BINDING(TYR_BINDING_MESH, 0) StructuredBuffer<Mesh> meshes : register(t1);
TYR_VK_BINDING(TYR_BINDING_MESH_LOD, 0) StructuredBuffer<MeshLOD> meshLODs : register(t2);
TYR_VK_BINDING(TYR_BINDING_MESHLET, 0) StructuredBuffer<Meshlet> meshlets : register(t3);
TYR_VK_BINDING(TYR_BINDING_MESH_INSTANCE, 0) StructuredBuffer<MeshInstance> meshInstances : register(t6);
// Written by the culling pass, indexed by this draw's slot through the DrawIndex builtin below,
// since a single indirect multi-draw call can't vary push constants per sub-draw.
TYR_VK_BINDING(TYR_BINDING_MESH_DRAWS, 0) StructuredBuffer<MeshDraw> meshDraws : register(t16);

#if TYR_LATE_CULL
	static const uint c_DrawBase = TYR_MAX_MESH_INSTANCES;
#else
	static const uint c_DrawBase = 0;
#endif

groupshared MeshTaskPayload s_Payload;
groupshared uint s_MeshletCount;

#ifndef TYR_VULKAN
// D3D12 has no DrawIndex builtin, so the indirect command signature sets this per draw instead.
cbuffer DrawConstants : register(b0, space1)
{
	uint g_DrawIndex;
};
#endif

[numthreads(TYR_MESHLETS_PER_TASK_GROUP, 1, 1)]
#ifdef TYR_VULKAN
void main(uint3 groupId : SV_GroupID, uint groupThreadIndex : SV_GroupIndex, [[vk::builtin("DrawIndex")]] uint drawIndex : DRAWINDEX)
#else
void main(uint3 groupId : SV_GroupID, uint groupThreadIndex : SV_GroupIndex)
#endif
{
#ifndef TYR_VULKAN
	const uint drawIndex = g_DrawIndex;
#endif
	const MeshDraw draw = meshDraws[c_DrawBase + drawIndex];
	const MeshInstance instance = meshInstances[draw.meshInstanceIndex];
	const Mesh mesh = meshes[instance.meshIndex];
	const MeshLOD lod = meshLODs[mesh.lodOffset + draw.lodIndex];

	// The last group can have fewer meshlets than threads - those threads re-read the last one.
	const uint localMeshletIndex = groupId.x * TYR_MESHLETS_PER_TASK_GROUP + groupThreadIndex;
	const bool inRange = localMeshletIndex < lod.meshletCount;
	const uint meshletIndex = lod.meshletOffset + min(localMeshletIndex, lod.meshletCount - 1u);
	const Meshlet meshlet = meshlets[meshletIndex];

	const float scaleSqr = MaxScaleSqr(instance.transform);
	const float3 centre = mul(float4(meshlet.center, 1.0f), instance.transform).xyz;
	const float radius = meshlet.radius * sqrt(scaleSqr);

	// Back-facing if every triangle faces away from the camera, using meshoptimizer's sphere-based
	// cone test. The cone only stays valid in world space under an even scale.
	const int4 coneBytes = asint(uint4(meshlet.cone << 24, meshlet.cone << 16, meshlet.cone << 8, meshlet.cone)) >> 24;
	const float4 cone = float4(coneBytes) / 127.0f;
	const float3 coneAxis = normalize(mul(float4(cone.xyz, 0.0f), instance.transform).xyz);
	const float3 row0 = float3(instance.transform._11, instance.transform._12, instance.transform._13);
	const float3 row1 = float3(instance.transform._21, instance.transform._22, instance.transform._23);
	const float3 row2 = float3(instance.transform._31, instance.transform._32, instance.transform._33);
	const float minScaleSqr = min(dot(row0, row0), min(dot(row1, row1), dot(row2, row2)));
	const float3 toCentre = centre - camPos;
	const bool backFacing = and(minScaleSqr >= scaleSqr * 0.98f, dot(toCentre, coneAxis) >= cone.w * length(toCentre) + radius);

	bool visible = and(IsSphereInFrustum(centre, radius), !backFacing);
#if TYR_LATE_CULL
	visible = and(visible, IsBoxVisibleInHiZ(centre, radius.xxx));
#endif

	// A skinned instance's meshlet bounds are its bind pose, so its meshlets are never culled.
	// Its draw only gets here if the instance's posed bounds passed.
	const bool skinned = instance.skinnedVertexOffset != TYR_INVALID_INDEX;
	visible = and(inRange, or(visible, skinned));

	s_MeshletCount = 0;
	s_Payload.meshInstanceIndex = draw.meshInstanceIndex;
	s_Payload.globalVertexBase = skinned ? instance.skinnedVertexOffset : lod.vertexOffset;
	s_Payload.prevGlobalVertexBase = skinned ? instance.prevSkinnedVertexOffset : lod.vertexOffset;
	s_Payload.globalIndexBase = lod.indexOffset;
	GroupMemoryBarrierWithGroupSync();

	if (visible)
	{
		uint slot;
		InterlockedAdd(s_MeshletCount, 1, slot);
		s_Payload.meshletIndices[slot] = meshletIndex;
	}
	GroupMemoryBarrierWithGroupSync();

	DispatchMesh(s_MeshletCount, 1, 1, s_Payload);
}

#endif
