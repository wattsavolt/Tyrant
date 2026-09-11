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
		static constexpr size_t c_ShaderIndexSize = sizeof(uint);
		static const size_t c_ShaderVertexSize;
		static const size_t c_ShaderMeshletSize;
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