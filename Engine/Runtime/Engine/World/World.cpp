#include "World.h"
#include "Camera.h"
#include "Math/Quaternion.h"

namespace tyr
{
	World::World()
		: m_SceneIndex(UINT8_MAX)
		, m_Camera(nullptr)
		, m_Active(true)
		, m_Visible(true)
		, m_Initialized(false)
	{

	}

	World::~World()
	{
		
	}

	void World::Initialize(const WorldConfig& config, uint8 sceneIndex)
	{
		TYR_ASSERT(!m_Initialized);

		m_Name = config.name;
		m_Camera = config.camera;
		m_ViewArea = config.viewArea;

		m_SceneIndex = sceneIndex;

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

