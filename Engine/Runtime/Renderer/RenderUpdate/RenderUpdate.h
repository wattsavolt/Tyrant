#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "RenderAPI/RenderAPITypes.h"
#include "Rendering/Scene.h"

namespace tyr
{
	struct RenderUpdateData
	{
		SceneAction sceneActions[Scene::c_MaxScenes];
	};
}