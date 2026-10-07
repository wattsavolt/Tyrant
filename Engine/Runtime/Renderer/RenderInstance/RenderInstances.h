#pragma once

#include "RenderInstanceDescs.h"

namespace tyr
{
	struct MeshInstance
	{
		MeshInstanceInfo info;
		uint renderIndex = c_InvalidRenderIndex;
	};

	struct SkeletalMeshInstance
	{
		SkeletalMeshInstanceInfo info;
		uint renderIndex = c_InvalidRenderIndex;
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
}