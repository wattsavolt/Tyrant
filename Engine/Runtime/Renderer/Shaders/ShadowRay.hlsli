#ifndef SHADOW_RAY_HLSli
#define SHADOW_RAY_HLSli

// Expects meshes, meshLODs, meshlets, vertices, indices, meshInstances, materials, textures and
// samplers to be declared before it's included.

// Whether a candidate hit on a non-opaque instance blocks light. Only instances with a masked or
// translucent material are non-opaque. Masked hits are alpha tested and translucent ones let light
// through. The hit triangle's material comes from the meshlet holding it - the acceleration
// structures hold LOD0, whose meshlets are in index order.
bool BlocksLight(uint meshInstanceIndex, uint primitiveIndex, float2 barycentrics)
{
	const MeshInstance instance = meshInstances[meshInstanceIndex];
	const MeshLOD lod = meshLODs[meshes[instance.meshIndex].lodOffset];
	const uint firstIndex = primitiveIndex * 3;

	uint low = 0;
	uint high = lod.meshletCount - 1;
	while (low < high)
	{
		const uint middle = (low + high + 1) / 2;
		const bool atOrBefore = meshlets[lod.meshletOffset + middle].indexOffset <= firstIndex;
		low = atOrBefore ? middle : low;
		high = atOrBefore ? high : middle - 1;
	}
	const Material material = materials[instance.materialIndices[meshlets[lod.meshletOffset + low].materialSlot]];

	// A skinned instance's acceleration structure is built from its skinned vertices.
	const bool skinned = instance.skinnedVertexOffset != TYR_INVALID_INDEX;
	const uint vertexBase = skinned ? instance.skinnedVertexOffset : lod.vertexOffset;
	const uint indexBase = lod.indexOffset + firstIndex;
	const float2 uv0 = (float2)vertices[vertexBase + indices[indexBase]].uv;
	const float2 uv1 = (float2)vertices[vertexBase + indices[indexBase + 1]].uv;
	const float2 uv2 = (float2)vertices[vertexBase + indices[indexBase + 2]].uv;
	const float2 uv = (uv0 * (1.0f - barycentrics.x - barycentrics.y) + uv1 * barycentrics.x + uv2 * barycentrics.y) * material.uvScale + material.uvOffset;

	const float alpha = textures[material.texture0].SampleLevel(samplers[0], uv, 0.0f).a * material.albedoTint.a;
	const uint blendMode = material.flags & TYR_MATERIAL_BLEND_MODE_MASK;
	return or(blendMode == TYR_MATERIAL_BLEND_MODE_OPAQUE, and(blendMode == TYR_MATERIAL_BLEND_MODE_MASKED, alpha >= material.alphaCutoff));
}

#endif
