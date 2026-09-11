#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "MeshDesc.h"
#include "RenderTransfer/BufferAllocation.h"

namespace tyr
{
	struct MeshLODAllocInfo
	{
		BufferAllocation vertexBufferAllocation{};
		BufferAllocation indexBufferAllocation{};
		BufferAllocation meshletBufferAllocation{};
	};

	struct Mesh
	{
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;
		uint lodOffset;
		uint lodCount;
	};

	struct SkeletalMesh
	{
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;
		uint lodOffset;
		uint lodCount;
	};
}