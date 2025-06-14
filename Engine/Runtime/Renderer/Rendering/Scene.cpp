#include "Scene.h"

namespace tyr
{
	Scene::Scene(SceneType type, SceneID sceneId, bool visible)
		: m_Type(type)
		, m_Id(sceneId)
		, m_Visible(visible)
	{
		
	}

	Scene::~Scene()
	{
	
	}
}