#pragma once

#include "RenderInstanceDescs.h"
#include "Geometry/BoundingSphere.h"
#include "RenderTransfer/BufferAllocation.h"
#include "RenderResource/RenderAccelerationStructure.h"

namespace tyr
{
	struct MeshInstance
	{
		MeshInstanceInfo info;
		// The scene it's drawn in, set when it's created.
		SceneHandle scene;
		uint renderIndex = c_InvalidRenderIndex;
	};

	// Drawn through its own mesh instance, whose vertices are skinned into this instance's
	// own range of the vertex buffer every frame.
	struct SkeletalMeshInstance
	{
		// Main thread only.
		SkeletalMeshInstanceInfo info;
		BoundingSphere skinnedSphere;
		SceneHandle scene;

		// Set at creation and never changed, so either thread can read them.
		MeshInstanceHandle meshInstance;
		MeshHandle mesh;
		BufferAllocation skinnedVertexAllocation{};
		// Vertex buffer element offsets: the mesh's rest pose, this frame's and last frame's skinned
		// vertices. The skin offset is into the skin vertex buffer.
		uint restVertexOffset = 0;
		uint skinVertexOffset = 0;
		uint outputVertexOffset = 0;
		uint prevOutputVertexOffset = 0;
		uint vertexCount = 0;
		// Rebuilt every frame from the skinned vertices, created on the main thread.
		RenderAccelerationStructure blas{};
		BufferAllocation blasStorageAllocation{};

		// Worker thread only.
		uint renderIndex = c_InvalidRenderIndex;
		uint skinnedFrameCount = 0;
	};

	struct DirectionalLight
	{
		DirectionalLightInfo info;
	};

	struct PointLight
	{
		PointLightInfo info;
	};

	struct SpotLight
	{
		SpotLightInfo info;
	};

	// Only ever touched on the main thread, which culls and sorts the decals each frame.
	struct Decal
	{
		DecalInfo info;
		// The box's bounding sphere in world space.
		Vector3 centre;
		float radius;
	};
}