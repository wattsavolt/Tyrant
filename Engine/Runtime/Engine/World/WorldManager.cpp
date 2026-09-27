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
				// the pool slot could be freed by the time onCreated fires later, since
				// creation is asynchronous - so onCreated re-fetches by handle, validated,
				// rather than capturing a pointer/reference to it directly.
				World& world = m_WorldPool[worldHandle];
				const uint index = world.meshInstances.Size();
				WorldMeshInstance& tracked = world.meshInstances.ExpandOne();
				tracked.meshAssetID = meshComponent.mesh;

				// TODO: MeshComponent has no transform of its own yet (see its own comment) -
				// once one exists, or entities get a proper transform component lookup here,
				// use that instead of identity.
				m_AssetManager->CreateMeshInstance(meshComponent.mesh, Matrix4::c_Identity, meshComponent.materialOverrides,
					[this, worldHandle, index](MeshInstanceHandle handle, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materialAssetIDs)
					{
						// The world might have been removed while this was still loading -
						// nothing to write back to in that case (ShutdownWorld already
						// released everything it knew about; this instance is now an
						// orphan the renderer will just carry - see WorldMeshInstance's
						// own comment on this being a known, accepted gap for now).
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
				world.dirLights.Add(m_RendererAPI->CreateDirectionalLight(lightDesc));
			});
		}

		// The other way a resize can be detected - externally, by the OS/windowing system
		// (PCWindow's WM_SIZE, or a future platform equivalent) - rather than internally by the
		// renderer itself (handled in Renderer::Render). Always consumed (so a stale pending flag
		// never lingers), but only acted on if the window is still active - a WM_SIZE can fire as
		// part of a window being closed/destroyed, and recreating a swap chain for a window that's
		// going away rather than resizing crashes (there's nothing valid left to recreate against).
		// RemoveWindow already handles proper teardown for that case.
		const bool resizePending = world.osWindowHandle && m_WindowModule->ConsumeResizePending(world.osWindowHandle);
		if (resizePending && m_WindowModule->IsWindowActive(world.osWindowHandle))
		{
			m_RendererAPI->ResizeWindow(world.windowHandle);
		}

		// RenderFrame is per-frame buffered data (RendererModule double/triple-buffers it and
		// clears each slot as it cycles back around), not persistent state - so the active
		// scene index has to be re-supplied every frame or the renderer silently stops picking
		// this world's data up once the buffered slot it was last written to gets reused.
		m_RendererAPI->SetActiveScene(world.sceneHandle, world.visible);

		// TODO: Shoehorned flat ambient term until a proper scene/lighting-settings system
		// exists to make this configurable per world. Same per-frame resupply requirement as
		// SetActiveScene/AddView above - see their comments.
		m_RendererAPI->SetAmbient(0.15f);

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

		// Not calling SetSceneWindow here - it only ever applies to the active scene, and a
		// freshly created world isn't automatically the active one (see SetActiveWorld).
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
			m_RendererAPI->SetSceneWindow(world.windowHandle);
		}
	}

	void WorldManager::SetWorldWindow(Handle worldHandle, RenderWindowHandle windowHandle)
	{
		World& world = m_WorldPool[worldHandle];
		world.windowHandle = windowHandle;

		if (worldHandle == m_ActiveWorld)
		{
			m_RendererAPI->SetSceneWindow(windowHandle);
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
		// Mirrors UpdateWorld's CreateMeshInstance calls: delete the renderer-side instance
		// and release this world's share of each material's refcount (both only ever actually
		// requested once the instance resolved - see WorldMeshInstance's own comment), plus
		// release this world's reference to the underlying mesh asset. LoadMesh (and so the
		// mesh asset's own refcount) was incremented synchronously when CreateMeshInstance was
		// first called, regardless of whether the instance itself has resolved yet, so
		// DeleteMesh always needs calling here to balance it - not just when meshInstance is
		// valid.
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
			m_RendererAPI->DeleteDirectionalLight(handle);
		}
		world.dirLights.Clear();

		m_RendererAPI->RemoveScene(world.sceneHandle);
		m_RendererAPI->RemoveWindow(world.windowHandle);
	}
}
