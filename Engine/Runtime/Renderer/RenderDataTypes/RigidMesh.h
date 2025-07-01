#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "Material.h"
#include "Resources/GpuBuffer.h"
#include "RenderDataTypesCommon.h"

namespace tyr
{
	struct RigidMeshInstance
	{
		// Accessor for the mesh instance data in the shader
		RenderDataAccessor renderDataAccessor;
		AssetID meshID;
		Matrix4 transform;
		// The material is not part of rigid mesh as it may be overwritten
		Material* material;
	};
}