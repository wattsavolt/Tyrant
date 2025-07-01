#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace tyr
{
	/// Directional light used in renderer 
	struct DirectionalLight
	{
		Vector3 direction;
		Vector3 diffuse;
		Vector3 specular;
	};

	/// Point light used in renderer 
	struct PointLight
	{
		Vector3 position;
		float range;
		Vector3 attenuation;
		Vector3 diffuse;
		Vector3 specular;
	};

	/// Spot light used in renderer 
	struct SpotLight
	{
		Vector3 position;
		float range;
		Vector3 direction;
		float cone;
		Vector3 attenuation;
		Vector4 diffuse;
		Vector3 specular;
	};
}