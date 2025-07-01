#pragma once

#include "RigidMesh.h"

namespace tyr
{
	struct ColourMeshInstance
	{
		RenderDataAccessor renderDataAccessor;
		Matrix4 transform;
		Vector4 colour;
	};
}