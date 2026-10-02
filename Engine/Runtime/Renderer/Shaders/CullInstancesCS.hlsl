#ifndef CULL_INSTANCES_CS_HLSli
#define CULL_INSTANCES_CS_HLSli

#include "Common.hlsli"

// The only shader that needs frustumPlanes, so it's the only one declaring the cbuffer's
// full field list.
[[vk::binding(TYR_BINDING_SCENE_INFO, 0)]]
cbuffer SceneInfoCBuffer : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3 camPos;
	float ambient;
	uint dirLightCount;
	uint pointLightCount;
	uint spotLightCount;
	float _pad0;
	float4 FrustumPlanes[6];
};

[[vk::binding(TYR_BINDING_MESH, 0)]] StructuredBuffer<Mesh> meshes : register(t1);
[[vk::binding(TYR_BINDING_MESH_LOD, 0)]] StructuredBuffer<MeshLOD> meshLODs : register(t2);
[[vk::binding(TYR_BINDING_MESH_INSTANCE, 0)]] StructuredBuffer<MeshInstance> meshInstances : register(t6);

[[vk::binding(TYR_BINDING_ACTIVE_INSTANCE_INDICES, 0)]] StructuredBuffer<uint> activeInstanceIndices : register(t15);
[[vk::binding(TYR_BINDING_VISIBLE_INSTANCE_INDICES, 0)]] RWStructuredBuffer<uint> visibleInstanceIndices : register(u16);

// Matches VkDrawMeshTasksIndirectCommandEXT byte-for-byte (3x uint32, tightly packed).
struct DrawMeshTasksIndirectCommand
{
	uint groupCountX;
	uint groupCountY;
	uint groupCountZ;
};
[[vk::binding(TYR_BINDING_INDIRECT_DRAW_COMMANDS, 0)]] RWStructuredBuffer<DrawMeshTasksIndirectCommand> indirectDrawCommands : register(u17);
[[vk::binding(TYR_BINDING_DRAW_COUNT, 0)]] RWStructuredBuffer<uint> drawCount : register(u18);

struct PushConstants
{
	uint activeInstanceCount;
};
[[vk::push_constant]] PushConstants g_PushConstants;

// Culled if the sphere is fully on the outside of any plane - dot(pos,abc)+d >= 0 means
// "inside" that plane.
bool IsSphereVisible(float3 centre, float radius)
{
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		const float4 plane = FrustumPlanes[i];
		if (dot(centre, plane.xyz) + plane.w < -radius)
		{
			return false;
		}
	}
	return true;
}

// Same idea as IsSphereVisible, for a world-space AABB given as centre + half-extents
// (Arvo's method). The box's "radius" along a plane's normal is dot(abs(plane.xyz),
// halfExtents) - each axis' half-extent weighted by its contribution to that normal.
bool IsBoxVisible(float3 centre, float3 halfExtents)
{
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		const float4 plane = FrustumPlanes[i];
		const float radius = dot(abs(plane.xyz), halfExtents);
		if (dot(centre, plane.xyz) + plane.w < -radius)
		{
			return false;
		}
	}
	return true;
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
	if (mesh.lodCount == 0)
	{
		return;
	}

	// LOD selection isn't implemented yet - always use LOD 0.
	const MeshLOD lod = meshLODs[mesh.lodOffset];
	if (lod.meshletCount == 0)
	{
		return;
	}

	// Mesh::sphere is in local space (xyz = centre, w = radius) - transform the centre with
	// the same row-vector convention used elsewhere, and scale the radius by the largest
	// per-axis scale so it stays conservative under non-uniform scale.
	const float3 row0 = float3(instance.transform._11, instance.transform._12, instance.transform._13);
	const float3 row1 = float3(instance.transform._21, instance.transform._22, instance.transform._23);
	const float3 row2 = float3(instance.transform._31, instance.transform._32, instance.transform._33);

	const float3 worldCentre = mul(float4(mesh.sphere.xyz, 1.0f), instance.transform).xyz;
	const float maxScaleSqr = max(dot(row0, row0), max(dot(row1, row1), dot(row2, row2)));
	const float worldRadius = mesh.sphere.w * sqrt(maxScaleSqr);

	// Sphere is cheap and rotation-invariant, so it's the first reject - most off-screen/far
	// instances stop here. It's loose for elongated meshes though, so anything it doesn't
	// reject gets refined with the mesh's AABB below.
	if (!IsSphereVisible(worldCentre, worldRadius))
	{
		return;
	}

	// Arvo's method: transform the box centre normally, and get each world-space half-extent
	// by dotting the local half-extents against the absolute value of that axis' row. Cheaper
	// than transforming all 8 corners, at the cost of a conservative (not tight) re-fit.
	const float3 aabbCentre = (mesh.aabbMin + mesh.aabbMax) * 0.5f;
	const float3 aabbHalfExtents = (mesh.aabbMax - mesh.aabbMin) * 0.5f;
	const float3 worldBoxCentre = mul(float4(aabbCentre, 1.0f), instance.transform).xyz;
	const float3 worldHalfExtents = float3(
		dot(abs(row0), aabbHalfExtents),
		dot(abs(row1), aabbHalfExtents),
		dot(abs(row2), aabbHalfExtents));

	if (!IsBoxVisible(worldBoxCentre, worldHalfExtents))
	{
		return;
	}

	uint drawSlot;
	InterlockedAdd(drawCount[0], 1, drawSlot);

	visibleInstanceIndices[drawSlot] = meshInstanceIndex;

	DrawMeshTasksIndirectCommand cmd;
	cmd.groupCountX = lod.meshletCount;
	cmd.groupCountY = 1;
	cmd.groupCountZ = 1;
	indirectDrawCommands[drawSlot] = cmd;
}

#endif
