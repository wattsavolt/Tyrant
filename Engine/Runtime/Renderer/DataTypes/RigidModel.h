#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "Material.h"
#include "Resources/GpuBuffer.h"
#include "DataTypesCommon.h"

namespace tyr
{
	/// Mesh object that is passed to the renderer 
	struct RigidMesh
	{
		AssetID id;
		Ref<GpuBuffer> data;
	};

	struct RigidMeshInstance
	{
		RenderableID id;
		Matrix4 transform;
		// The material is not part of rigid mesh as it may be overwritten
		Ref<Material> material;
		Ref<RigidMesh> mesh;
	};
}