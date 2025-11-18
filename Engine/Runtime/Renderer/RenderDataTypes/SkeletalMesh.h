#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"
#include "Material.h"
#include "Vertex.h"
#include "Resources/RenderBuffer.h"
#include "RenderDataTypesCommon.h"

namespace tyr
{
	struct SkeletalMeshInstance
	{
		// Accessor for the mesh instance data int the shader
		RenderDataAccessor renderDataAccessor;
		AssetID meshID;
		AssetID skeletonID;
		Matrix4 transform;
		// The material is not part of rigid mesh as it may be overwritten
		Material* material;
	};
}