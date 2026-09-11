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
		uint renderIndex = c_InvalidRenderIndex;
	};

	struct PointLight
	{
		PointLightInfo info;
		uint renderIndex = c_InvalidRenderIndex;
	};

	struct SpotLight
	{
		SpotLightInfo info;
		uint renderIndex = c_InvalidRenderIndex;
	};
}