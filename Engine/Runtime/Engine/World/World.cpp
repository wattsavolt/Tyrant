#include "World.h"
#include "Camera.h"
#include "Math/Quaternion.h"

namespace tyr
{
	World::World()
		: m_Initialized(false)
		, m_IsFirstFrame(true)
		, m_Camera(nullptr)
	{

	}

	World::~World()
	{
		
	}

	void World::Initialize(const WorldParams& params)
	{
		TYR_ASSERT(!m_Initialized);
		TYR_ASSERT(params.camera != nullptr);
		m_ViewArea = params.viewArea;
		m_Camera = params.camera;
		m_IsFirstFrame = true;
		m_Initialized = true;
	}

	void World::Shutdown()
	{
		TYR_ASSERT(m_Initialized);
		m_Initialized = false;
	}

	Camera* World::GetCamera() const
	{ 
		return m_Camera; 
	}

	void World::SetCamera(Camera* camera)
	{
		m_Camera = camera;
	}
}

