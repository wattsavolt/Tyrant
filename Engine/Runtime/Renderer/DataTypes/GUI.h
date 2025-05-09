#pragma once

#include "RigidModel.h"

namespace tyr
{
	struct ColourMeshInstance
	{
		Matrix4 transform;
		Vector4 colour;
		Ref<RigidMesh> mesh;
	};
}