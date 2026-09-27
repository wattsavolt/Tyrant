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
		// The resource queue's timeline value that needs to be reached before this mesh's
		// geometry is safely readable - 0 means its transfer hasn't been submitted yet.
		uint64 geometryReadyValue = 0;
	};

	struct SkeletalMesh
	{
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;
		uint lodOffset;
		uint lodCount;
		uint64 geometryReadyValue = 0;
	};
}