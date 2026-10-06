#include "WorldManager.h"
#include "BuildConfig.h"
#include "Camera.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"
#include "RenderInstance/RenderInstanceDescs.h"
#include "Window/WindowModule.h"
#include "AssetSystem/AssetModule.h"
#include "AssetSystem/AssetManager.h"
#include "ECS/Components.h"

namespace tyr
{
	WorldManager::WorldManager()
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();

		TYR_GET_MODULE(WindowModule, m_WindowModule);

		AssetModule* assetModule;
		TYR_GET_MODULE(AssetModule, assetModule);
		m_AssetManager = assetModule->GetAssetManager();
	}

	WorldManager::~WorldManager()
	{
		RemoveWorlds();
	}

	void WorldManager::Update(float deltaTime)
	{
		if (!m_ActiveWorld)
		{
			return;
		}

		World& world = m_WorldPool[m_ActiveWorld];
		UpdateWorld(m_ActiveWorld, world, deltaTime);
	}

	void WorldManager::UpdateWorld(Handle worldHandle, World& world, float deltaTime)
	{
		// Creates renderer-side mesh instances and lights for any components added since last time.
		if (world.syncedEntitiesVersion != world.entities.GetVersion())
		{
			world.syncedEntitiesVersion = world.entities.GetVersion();

			world.entities.ForEach<MeshComponent>([this, worldHandle](Entity entity, MeshComponent& meshComponent)
			{
				if (!meshComponent.meshInstanceRequested)
				{
					SyncMeshInstance(worldHandle, entity, meshComponent);
				}
			});

			world.entities.ForEach<DirLightComponent>([&world, this](Entity /*entity*/, DirLightComponent& lightComponent)
			{
				if (lightComponent.lightHandle)
				{
					return;
				}
				DirectionalLightDesc lightDesc{};
				lightDesc.info.direction = lightComponent.direction;
				lightDesc.info.colour = lightComponent.colour;
				lightDesc.info.intensity = lightComponent.intensity;
				lightDesc.info.castsShadow = lightComponent.castsShadow;
				lightComponent.lightHandle = m_RendererAPI->CreateDirectionalLight(world.sceneHandle, lightDesc);
			});
		}

		// The other way a resize can be detected - externally, by the OS/windowing system -
		// rather than internally by the renderer itself. Always consumed, but only acted on if
		// the window is still active, since a resize can fire as part of the window being destroyed.
		const bool resizePending = world.osWindowHandle && m_WindowModule->ConsumeResizePending(world.osWindowHandle);
		if (resizePending && m_WindowModule->IsWindowActive(world.osWindowHandle))
		{
			m_RendererAPI->ResizeWindow(world.windowHandle);
		}

		// RenderFrame is per-frame buffered data, not persistent state - the active scene index
		// has to be re-supplied every frame or the renderer silently stops picking this
		// world's data up once the buffered slot it was last written to gets reused.
		m_RendererAPI->SetActiveScene(world.sceneHandle, world.visible);

		// TODO: Shoehorned flat ambient term until a proper scene/lighting-settings system
		// exists to make this configurable per world. Called here every tick anyway since
		// nothing else makes this value change yet.
		m_RendererAPI->SetSceneAmbient(world.sceneHandle, 0.15f);

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
		world.windowHandle = config.windowHandle;
		world.osWindowHandle = config.osWindowHandle;

		world.sceneHandle = m_RendererAPI->AddScene(world.name.CStr());
		// Unlike windowHandle/sceneHandle's own setup, a RenderViewport needs no OS window and
		// nothing depends on this world being the active one yet - safe to create it for
		// every world immediately.
		world.renderViewportHandle = m_RendererAPI->CreateRenderViewport();

		// Not calling the active-scene-window/viewport setup here - that only applies to the
		// active scene, and a freshly created world isn't automatically the active one.
	}

	void WorldManager::SetActiveWorld(Handle worldHandle)
	{
		if (m_ActiveWorld == worldHandle)
		{
			return;
		}

		m_ActiveWorld = worldHandle;

		if (worldHandle)
		{
			const World& world = m_WorldPool[worldHandle];
			m_RendererAPI->SetSceneWindow(world.sceneHandle, world.windowHandle);
			m_RendererAPI->SetSceneRenderViewport(world.sceneHandle, world.renderViewportHandle);
		}
	}

	void WorldManager::SetWorldWindow(Handle worldHandle, RenderWindowHandle windowHandle)
	{
		World& world = m_WorldPool[worldHandle];
		world.windowHandle = windowHandle;

		if (worldHandle == m_ActiveWorld)
		{
			m_RendererAPI->SetSceneWindow(world.sceneHandle, windowHandle);
		}
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
		if (worldHandle == m_ActiveWorld)
		{
			SetActiveWorld({});
		}

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
		m_ActiveWorld = {};

		for (Handle worldHandle : m_Worlds)
		{
			World& world = m_WorldPool[worldHandle];
			ShutdownWorld(world);
			m_WorldPool.Delete(worldHandle);
		}
		m_Worlds.Clear();
	}

	void WorldManager::SyncMeshInstance(Handle worldHandle, Entity entity, MeshComponent& meshComponent)
	{
		meshComponent.meshInstanceRequested = true;

		const AssetID meshID = meshComponent.mesh;

		Matrix4 transform = Matrix4::c_Identity;
		EntitySystem& entities = m_WorldPool[worldHandle].entities;
		if (entities.HasComponent<ComponentTransform>(entity))
		{
			const Transform& worldTransform = entities.GetComponent<ComponentTransform>(entity).world;
			transform = Matrix4::CreateTRS(worldTransform.position, worldTransform.rotation, worldTransform.scale);
		}

		m_AssetManager->CreateMeshInstance(meshID, transform, meshComponent.materials,
			[this, worldHandle, entity, meshID](MeshInstanceHandle handle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials)
			{
				// The entity or its world was removed while this was loading, so nothing owns it.
				if (!m_WorldPool.IsValid(worldHandle) || !m_WorldPool[worldHandle].entities.HasComponent<MeshComponent>(entity))
				{
					ReleaseMeshInstance(handle, meshID, materials);
					return;
				}

				MeshComponent& comp = m_WorldPool[worldHandle].entities.GetComponent<MeshComponent>(entity);
				comp.meshInstance = handle;
				comp.materials = materials;
			});
	}

	void WorldManager::RemoveMeshInstance(const MeshComponent& meshComponent)
	{
		// One that's still loading is released by its creation callback once it finishes.
		if (meshComponent.meshInstance)
		{
			ReleaseMeshInstance(meshComponent.meshInstance, meshComponent.mesh, meshComponent.materials);
		}
	}

	void WorldManager::ReleaseMeshInstance(MeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials)
	{
		m_RendererAPI->DeleteMeshInstance(handle);
		for (AssetID materialID : materials)
		{
			m_AssetManager->DeleteMaterial(materialID);
		}
		m_AssetManager->DeleteMesh(meshID);
	}

	void WorldManager::AddActorInstance(Handle worldHandle, const char* name, const char* folderPath, const LocalArray<Entity, c_MaxActorInstanceEntities>& entities)
	{
		World& world = m_WorldPool[worldHandle];

		ActorInstance& instance = world.actorInstances.ExpandOne();
		instance.name = name;
		instance.folderPath = folderPath;
		instance.entities = entities;
	}

	void WorldManager::RemoveActorInstance(Handle worldHandle, Entity rootEntity)
	{
		World& world = m_WorldPool[worldHandle];

		for (uint i = 0; i < world.actorInstances.Size(); ++i)
		{
			if (world.actorInstances[i].RootEntity() != rootEntity)
			{
				continue;
			}

			for (Entity entity : world.actorInstances[i].entities)
			{
				if (world.entities.HasComponent<MeshComponent>(entity))
				{
					RemoveMeshInstance(world.entities.GetComponent<MeshComponent>(entity));
				}
				world.entities.RemoveEntity(entity);
			}

			world.actorInstances.SwapAndPopBack(i);
			return;
		}
	}

	void WorldManager::ShutdownWorld(World& world)
	{
		world.entities.ForEach<MeshComponent>([this](Entity /*entity*/, MeshComponent& meshComponent)
		{
			RemoveMeshInstance(meshComponent);
		});

		world.entities.ForEach<DirLightComponent>([this, &world](Entity /*entity*/, DirLightComponent& lightComponent)
		{
			if (lightComponent.lightHandle)
			{
				m_RendererAPI->DeleteDirectionalLight(world.sceneHandle, lightComponent.lightHandle);
			}
		});

		m_RendererAPI->RemoveScene(world.sceneHandle);
		m_RendererAPI->DeleteRenderViewport(world.renderViewportHandle);
	}
}
