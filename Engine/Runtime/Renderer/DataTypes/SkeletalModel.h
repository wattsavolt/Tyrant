#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"
#include "Material.h"
#include "Vertex.h"
#include "Resources/GpuBuffer.h"
#include "DataTypesCommon.h"

namespace tyr
{
	// Mesh object that is passed to the renderer 
	struct SkeletalMesh
	{
		std::shared_ptr<Material> material;
		Ref<GpuBuffer> jointBuffer;
		Ref<GpuBuffer> weightBuffer;
		uint jointBufferOffset = 0u;
		uint weightBufferOffset = 0u;
		// TODO : Add joints
	};

	struct SkeletalModel
	{
		AssetID id;
		Array<SkeletalMesh> meshes;
	};

	struct SkeletalModelInstance
	{
		RenderableID id;
		Ref<SkeletalModel> model;
		Matrix4 transform;
		// TODO: Add bone transforms
	};
}