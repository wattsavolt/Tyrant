#ifndef CULL_INSTANCES_HLSli
#define CULL_INSTANCES_HLSli

// Two-pass occlusion culling. The early pass (TYR_LATE_CULL 0) draws what was visible last frame,
// tested against the frustum only. The late pass (TYR_LATE_CULL 1) tests everything against the
// Hi-Z built from the early pass, draws what the early pass missed and records what's visible.

#include "Culling.hlsli"

TYR_VK_BINDING(TYR_BINDING_MESH, 0) StructuredBuffer<Mesh> meshes : register(t1);
TYR_VK_BINDING(TYR_BINDING_MESH_LOD, 0) StructuredBuffer<MeshLOD> meshLODs : register(t2);
TYR_VK_BINDING(TYR_BINDING_MESH_INSTANCE, 0) StructuredBuffer<MeshInstance> meshInstances : register(t6);

TYR_VK_BINDING(TYR_BINDING_ACTIVE_INSTANCE_INDICES, 0) StructuredBuffer<uint> activeInstanceIndices : register(t15);
TYR_VK_BINDING(TYR_BINDING_MESH_DRAWS, 0) RWStructuredBuffer<MeshDraw> meshDraws : register(u16);

// Matches VkDrawMeshTasksIndirectCommandEXT byte-for-byte (3x uint32, tightly packed).
struct DrawMeshTasksIndirectCommand
{
	uint groupCountX;
	uint groupCountY;
	uint groupCountZ;
};
TYR_VK_BINDING(TYR_BINDING_INDIRECT_DRAW_COMMANDS, 0) RWStructuredBuffer<DrawMeshTasksIndirectCommand> indirectDrawCommands : register(u17);
// The early pass's draw count, then the late pass's.
TYR_VK_BINDING(TYR_BINDING_DRAW_COUNT, 0) RWStructuredBuffer<uint> drawCount : register(u18);
TYR_VK_BINDING(TYR_BINDING_INSTANCE_VISIBILITY, 0) RWStructuredBuffer<uint> instanceVisibility : register(u1, space2);

// The coarsest LOD whose error, projected onto the screen, stays within the allowed pixel error.
// LOD errors only grow, so the last one that fits is the coarsest.
uint SelectLod(uint lodBase, uint lodCount, float distance, float scale)
{
	const float errorToPixels = scale * g_PushConstants.lodScale / max(distance, 1e-4f);
	uint lodIndex = 0;
	[unroll]
	for (uint i = 1; i < TYR_MAX_LODS; ++i)
	{
		const float error = meshLODs[lodBase + min(i, lodCount - 1u)].error;
		const bool fits = and(i < lodCount, error * errorToPixels <= 1.0f);
		lodIndex = fits ? i : lodIndex;
	}
	return lodIndex;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint index = dispatchThreadID.x;
	if (index >= g_PushConstants.activeInstanceCount)
	{
		return;
	}

	const uint meshInstanceIndex = activeInstanceIndices[index];
	const MeshInstance instance = meshInstances[meshInstanceIndex];
	const Mesh mesh = meshes[instance.meshIndex];

	// Mesh::sphere is in local space (xyz = centre, w = radius). The radius is scaled by the
	// largest per-axis scale so it stays conservative under non-uniform scale. A skinned instance
	// is bounded by its current pose instead of the mesh's bind pose.
	const bool skinned = instance.skinnedVertexOffset != TYR_INVALID_INDEX;
	const float4 sphere = skinned ? instance.skinnedSphere : mesh.sphere;
	const float scale = sqrt(MaxScaleSqr(instance.transform));
	const float3 worldCentre = mul(float4(sphere.xyz, 1.0f), instance.transform).xyz;
	const float worldRadius = sphere.w * scale;

	// Arvo's method: each world-space half-extent is the local half-extents dotted with the
	// absolute value of that axis' row. A skinned instance's AABB is its bind pose, so it uses a
	// box around its posed sphere instead.
	const float3 aabbCentre = (mesh.aabbMin + mesh.aabbMax) * 0.5f;
	const float3 aabbHalfExtents = (mesh.aabbMax - mesh.aabbMin) * 0.5f;
	const float3 row0 = float3(instance.transform._11, instance.transform._12, instance.transform._13);
	const float3 row1 = float3(instance.transform._21, instance.transform._22, instance.transform._23);
	const float3 row2 = float3(instance.transform._31, instance.transform._32, instance.transform._33);
	const float3 worldHalfExtents = float3(dot(abs(row0), aabbHalfExtents), dot(abs(row1), aabbHalfExtents), dot(abs(row2), aabbHalfExtents));
	const float3 boxCentre = skinned ? worldCentre : mul(float4(aabbCentre, 1.0f), instance.transform).xyz;
	const float3 boxHalfExtents = skinned ? worldRadius.xxx : worldHalfExtents;

	const bool inFrustum = and(IsSphereInFrustum(worldCentre, worldRadius), IsBoxInFrustum(boxCentre, boxHalfExtents));

	// A mesh with no LODs reads the first entry instead of going out of range, and is never drawn.
	// Skinned instances only have their LOD0 vertices skinned.
	const bool hasLod = mesh.lodCount > 0;
	const uint lodBase = hasLod ? mesh.lodOffset : 0;
	const float distance = max(length(worldCentre - camPos) - worldRadius, 0.0f);
	const uint lodIndex = skinned ? 0 : SelectLod(lodBase, mesh.lodCount, distance, scale);
	const MeshLOD lod = meshLODs[lodBase + lodIndex];

	const bool hasGeometry = and(hasLod, lod.meshletCount > 0);
	const bool drawnEarly = and(and(hasGeometry, inFrustum), instanceVisibility[meshInstanceIndex] != 0);

#if TYR_LATE_CULL
	const bool visible = and(and(hasGeometry, inFrustum), IsBoxVisibleInHiZ(boxCentre, boxHalfExtents));
	instanceVisibility[meshInstanceIndex] = (uint)visible;
	const bool draw = and(visible, !drawnEarly);
	const uint countIndex = 1;
	const uint drawBase = TYR_MAX_MESH_INSTANCES;
#else
	const bool draw = drawnEarly;
	const uint countIndex = 0;
	const uint drawBase = 0;
#endif

	if (!draw)
	{
		return;
	}

	uint drawSlot;
	InterlockedAdd(drawCount[countIndex], 1, drawSlot);

	MeshDraw meshDraw;
	meshDraw.meshInstanceIndex = meshInstanceIndex;
	meshDraw.lodIndex = lodIndex;
	meshDraws[drawBase + drawSlot] = meshDraw;

	DrawMeshTasksIndirectCommand cmd;
	cmd.groupCountX = (lod.meshletCount + TYR_MESHLETS_PER_TASK_GROUP - 1) / TYR_MESHLETS_PER_TASK_GROUP;
	cmd.groupCountY = 1;
	cmd.groupCountZ = 1;
	indirectDrawCommands[drawBase + drawSlot] = cmd;
}

#endif
