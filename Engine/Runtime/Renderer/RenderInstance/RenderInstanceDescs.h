#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "Math/Quaternion.h"
#include "RenderBase/RenderHandles.h"
#include "RenderResource/MeshDesc.h"

namespace tyr
{
	static constexpr uint c_InvalidRenderIndex = UINT32_MAX;

	struct MeshInstanceInfo
	{
		Matrix4 transform;
		MeshHandle mesh;
		// One resolved material per submesh slot - see ShaderMeshInstance::materialIndices'
		// comment (Shaders/ShaderTypes.h) for how these get decided (mesh defaults, per-slot
		// overridden by a MeshComponent where present).
		LocalArray<MaterialHandle, MeshConstants::c_MaxSubmeshes> materials;
		// Scales its materials' emissive, such as 0 for a lamp that's switched off.
		float emissiveScale = 1.0f;
	};

	struct SkeletalMeshInstanceInfo
	{
		Matrix4 transform;
		// Must be a skinned mesh whose geometry has finished uploading.
		MeshHandle mesh;
		// One resolved material per submesh slot, like MeshInstanceInfo::materials.
		LocalArray<MaterialHandle, MeshConstants::c_MaxSubmeshes> materials;
	};

	struct DirectionalLightInfo
	{
		Vector3 direction;
		Vector3 colour;
		float intensity;
		bool castsShadow;
	};

	struct PointLightInfo
	{
		Vector3 position;
		Vector3 attenuation;
		Vector3 colour;
		float intensity;
		float range;
		bool castsShadow;
	};

	struct SpotLightInfo
	{
		Vector3 position;
		Vector3 direction;
		Vector3 colour;
		Vector3 attenuation;
		float intensity;
		float range;
		float cone;
		bool castsShadow;
	};

	struct MeshInstanceDesc
	{
		MeshInstanceInfo info;
	};

	struct SkeletalMeshInstanceDesc
	{
		SkeletalMeshInstanceInfo info;
	};

	struct DirectionalLightDesc
	{
		DirectionalLightInfo info;
	};

	struct PointLightDesc
	{
		PointLightInfo info;
	};

	struct SpotLightDesc
	{
		SpotLightInfo info;
	};

	// A unit box placed by the transform, projecting the material along its -Y axis.
	struct DecalInfo
	{
		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
		MaterialHandle material;
		float opacity;
		// How much of the box's depth, from each end, it fades out over, from 0 to 1.
		float fadeDepth;
		bool affectsColour;
		bool affectsNormal;
		bool affectsRoughness;
		// Higher draws over lower.
		int sortOrder;
	};

	struct DecalDesc
	{
		DecalInfo info;
	};
}