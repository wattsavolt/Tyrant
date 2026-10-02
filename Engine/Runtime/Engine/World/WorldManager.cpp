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
		if (!world.meshInstancesSynced)
		{
			world.meshInstancesSynced = true;
			world.entities.ForEach<MeshComponent>([this, worldHandle](Entity /*entity*/, MeshComponent& meshComponent)
			{
				// world (the reference captured just below) only stays valid for this call -
				// the pool slot could be freed by the time onCreated fires later, so it
				// re-fetches by handle, validated, rather than capturing a reference directly.
				World& world = m_WorldPool[worldHandle];
				const uint index = world.meshInstances.Size();
				WorldMeshInstance& tracked = world.meshInstances.ExpandOne();
				tracked.meshAssetID = meshComponent.mesh;

				// TODO: MeshComponent has no transform of its own yet - once one exists, or
				// entities get a proper transform component lookup here, use that instead of identity.
				m_AssetManager->CreateMeshInstance(meshComponent.mesh, Matrix4::c_Identity, meshComponent.materialOverrides,
					[this, worldHandle, index](MeshInstanceHandle handle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materialAssetIDs)
					{
						// The world might have been removed while this was still loading -
						// nothing to write back to in that case; this instance becomes an
						// orphan the renderer just carries.
						if (m_WorldPool.IsValid(worldHandle))
						{
							WorldMeshInstance& tracked = m_WorldPool[worldHandle].meshInstances[index];
							tracked.meshInstance = handle;
							tracked.materialAssetIDs = materialAssetIDs;
						}
					});
			});
		}

		if (!world.dirLightsSynced)
		{
			world.dirLightsSynced = true;
			// Unlike mesh instances, a directional light is created synchronously - no need for
			// an onCreated-style callback, so the handle can just be captured directly here.
			world.entities.ForEach<DirLightComponent>([&world, this](Entity /*entity*/, DirLightComponent& lightComponent)
			{
				DirectionalLightDesc lightDesc{};
				lightDesc.info.direction = lightComponent.direction;
				lightDesc.info.colour = lightComponent.colour;
				lightDesc.info.intensity = lightComponent.intensity;
				lightDesc.info.castsShadow = lightComponent.castsShadow;
				world.dirLights.Add(m_RendererAPI->CreateDirectionalLight(world.sceneHandle, lightDesc));
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

	void WorldManager::ShutdownWorld(World& world)
	{
		// Delete the renderer-side instance and release this world's share of each material's
		// refcount once resolved, plus release the mesh asset reference itself regardless of
		// whether the instance ever resolved.
		for (const WorldMeshInstance& tracked : world.meshInstances)
		{
			if (tracked.meshInstance)
			{
				m_RendererAPI->DeleteMeshInstance(tracked.meshInstance);
				for (AssetID materialAssetID : tracked.materialAssetIDs)
				{
					m_AssetManager->DeleteMaterial(materialAssetID);
				}
			}
			m_AssetManager->DeleteMesh(tracked.meshAssetID);
		}
		world.meshInstances.Clear();

		for (DirLightHandle handle : world.dirLights)
		{
			m_RendererAPI->DeleteDirectionalLight(world.sceneHandle, handle);
		}
		world.dirLights.Clear();

		m_RendererAPI->RemoveScene(world.sceneHandle);
		m_RendererAPI->DeleteRenderViewport(world.renderViewportHandle);
		m_RendererAPI->RemoveWindow(world.windowHandle);
	}
}
