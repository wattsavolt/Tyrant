#include "World.h"
#include "Camera.h"
#include "Math/Quaternion.h"

namespace tyr
{
	uint8 World::s_NextSceneIndex = 0;

	World::World()
		: m_Initialized(false)
		, m_Camera(nullptr)
		, m_Active(true)
		, m_Visible(true)
	{

	}

	World::~World()
	{
		
	}

	void World::Initialize(const WorldParams& params)
	{
		TYR_ASSERT(!m_Initialized);

		m_Name = params.name;
		m_Camera = params.camera;
		m_ViewArea = params.viewArea;
		m_SceneIndex = s_NextSceneIndex++ % Scene::c_MaxScenes;

		m_Initialized = true;
	}

	void World::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		m_Initialized = false;
	}

	void World::Update(float deltaTime, SceneFrame& sceneFrame)
	{
		if (!m_Active)
		{
			return;
		}

		sceneFrame.visible = m_Visible;
		sceneFrame.view.viewArea = m_ViewArea;
		sceneFrame.view.camera.position = m_Camera->GetPosition();
		sceneFrame.view.camera.forward = m_Camera->GetForward();
		sceneFrame.view.camera.up = m_Camera->GetUp();
		sceneFrame.view.camera.fov = m_Camera->GetFOV();
		sceneFrame.view.camera.nearZ = m_Camera->GetNearZ();
		sceneFrame.view.camera.farZ = m_Camera->GetFarZ();

		// TODO: Add other render frame data
	}

	Camera* World::GetCamera() const
	{ 
		return m_Camera; 
	}

	void World::SetCamera(Camera* camera)
	{
		m_Camera = camera;
	}

	void World::SetActive(bool active)
	{
		m_Active = active;
	}

	void World::SetVisible(bool visible)
	{
		m_Visible = visible;
	}
}

