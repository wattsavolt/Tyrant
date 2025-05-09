#include "Pass.h"

namespace tyr
{
	Pass::Pass(SceneId sceneId)
		: m_SceneId(Scene::c_InvalidID)
	{
	
	}

	Pass::~Pass()
	{
	
	}

	ShaderPass::ShaderPass(Scene* scene)
		: Pass(scene->GetId())
		, m_Scene(scene)
	{

	}

	ShaderPass::~ShaderPass()
	{

	}
}