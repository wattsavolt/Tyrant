#include "WorldManager.h"
#include "BuildConfig.h"
#include "Camera.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"

namespace tyr
{
	WorldManager::WorldManager()
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();
	}

	WorldManager::~WorldManager()
	{
		RemoveWorlds();
	}

	void WorldManager::Update(float deltaTime)
	{
		for (Handle worldHandle : m_Worlds)
		{
			World& world = m_WorldPool[worldHandle];
			UpdateWorld(world, deltaTime);
		}
	}

	void WorldManager::UpdateWorld(World& world, float deltaTime)
	{
		if (!world.active)
		{
			return;
		}

		m_RendererAPI->SetActiveSceneIndex(world.sceneIndex, world.visible);

		SceneView view;
		view.viewArea = world.viewArea;
		view.camera.position = world.camera->GetPosition();
		view.camera.forward = world.camera->GetForward();
		view.camera.up = world.camera->GetUp();
		view.camera.fov = world.camera->GetFOV();
		view.camera.nearZ = world.camera->GetNearZ();
		view.camera.farZ = world.camera->GetFarZ();

		m_RendererAPI->AddView(view);
	}

	Handle WorldManager::AddWorld(const WorldConfig& params)
	{
		const Handle worldHandle = m_WorldPool.Create();
		World& world = m_WorldPool[worldHandle];
		InitWorld(world, params);
		m_Worlds.Add(worldHandle);
		return worldHandle;
	}

	void WorldManager::InitWorld(World& world, const WorldConfig& config)
	{
		world.name = config.name;
		world.camera = config.camera;
		world.viewArea = config.viewArea;

		world.sceneIndex = m_RendererAPI->AddScene(world.name.CStr());
	}

	const World& WorldManager::GetWorld(Handle worldHandle) const
	{
		return m_WorldPool[worldHandle];
	}

	World& WorldManager::GetWorld(Handle worldHandle)
	{
		return m_WorldPool[worldHandle];
	}

	void WorldManager::RemoveWorld(Handle worldHandle)
	{
		World& world = m_WorldPool[worldHandle];
		ShutdownWorld(world);
		for (uint i = 0; i < m_Worlds.Size(); ++i)
		{
			if (worldHandle == m_Worlds[i])
			{
				m_Worlds.Erase(i);
				break;
			}
		}
		m_WorldPool.Delete(worldHandle);
	}

	void WorldManager::RemoveWorlds()
	{
		for (Handle worldHandle : m_Worlds)
		{
			World& world = m_WorldPool[worldHandle];
			ShutdownWorld(world);
			m_WorldPool.Delete(worldHandle);
		}
		m_Worlds.Clear();
	}

	void WorldManager::ShutdownWorld(World& world)
	{
		m_RendererAPI->RemoveScene(world.sceneIndex);
	}
}
