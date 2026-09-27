#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Geometry/BoundingSphere.h"

namespace tyr
{
	struct TYR_RENDERER_API MeshConstants
	{
		static constexpr uint8 c_MaxLods = 5;
		// Per-submesh material slots a mesh instance can hold - must match
		// ShaderMeshInstance::materialIndices' array size in Shaders/ShaderTypes.h exactly,
		// since that's the actual GPU-side layout this is describing.
		static constexpr uint8 c_MaxSubmeshes = 16;
		static constexpr size_t c_ShaderIndexSize = sizeof(uint);
		static const size_t c_ShaderVertexSize;
		static const size_t c_ShaderMeshletSize;
	};

	struct TYR_RENDERER_API SkeletalMeshConstants
	{
		// Per-submesh material slots a skeletal mesh instance can hold - must match
		// ShaderSkeletalInstance::materialIndices' array size in Shaders/ShaderTypes.h
		// exactly. Higher than MeshConstants::c_MaxSubmeshes since skinned character
		// content (skin, eyes, hair, clothing layers, accessories) tends to use more
		// distinct materials than static props/environment pieces.
		static constexpr uint8 c_MaxSubmeshes = 24;
	};

	// The material is not part of the rigid or skeletal mesh as it may be overwritten
	struct MeshDesc
	{
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;
		uint lodCount;
	};

	struct SkeletalMeshDesc
	{
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;
		uint lodCount;
	};
}