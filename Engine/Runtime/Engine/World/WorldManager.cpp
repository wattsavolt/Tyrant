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
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "Reflection/TypeRegistry.h"
#include "Actor/ActorTypes.h"
#include "Platform/Platform.h"
#include "Debug/DebugDraw.h"
#include <cstring>

namespace tyr
{
	namespace
	{
		bool HasExtension(const char* path, const char* extension)
		{
			const char* pathExtension = strrchr(path, '.');
			return pathExtension && strcmp(pathExtension, extension) == 0;
		}

		// Whether an actor of this type, added to the world, keeps the world within max of T.
		template<typename T>
		bool HasRoomFor(EntitySystem& entities, const ActorTypeDesc& actorType, uint max)
		{
			uint count = 0;
			const Id64 typeID = GetTypeID<T>();
			for (const ActorEntityDesc& entity : actorType.entities)
			{
				for (const ActorComponentDesc& component : entity.components)
				{
					count += component.typeID == typeID ? 1 : 0;
				}
			}
			if (count == 0)
			{
				return true;
			}

			entities.ForEach<T>([&count](Entity /*entity*/, T& /*component*/)
			{
				++count;
			});
			return count <= max;
		}

		// The entity's world transform, or no transform if it has none.
		Transform GetWorldTransform(EntitySystem& entities, Entity entity)
		{
			if (entities.HasComponent<ComponentTransform>(entity))
			{
				return entities.GetComponent<ComponentTransform>(entity).world;
			}
			Transform transform;
			transform.position = Vector3::c_Zero;
			transform.rotation = Quaternion::c_Identity;
			transform.scale = Vector3::c_One;
			return transform;
		}

		DirectionalLightDesc CreateLightDesc(const DirLightComponent& light)
		{
			DirectionalLightDesc desc{};
			desc.info.direction = light.direction;
			desc.info.colour = light.colour;
			desc.info.intensity = light.intensity;
			desc.info.castsShadow = light.castsShadow;
			return desc;
		}

		PointLightDesc CreateLightDesc(const PointLightComponent& light, const Transform& transform)
		{
			PointLightDesc desc{};
			desc.info.position = transform.position;
			desc.info.attenuation = light.attenuation;
			desc.info.colour = light.colour;
			desc.info.intensity = light.intensity;
			desc.info.range = light.range;
			desc.info.castsShadow = light.castsShadow;
			return desc;
		}

		SpotLightDesc CreateLightDesc(const SpotLightComponent& light, const Transform& transform)
		{
			// Shines along the transform's Z axis, which is the rotation's third row like Matrix4::SetTRS.
			Matrix3 rotation;
			transform.rotation.ToRotationMatrix(rotation);

			SpotLightDesc desc{};
			desc.info.position = transform.position;
			desc.info.direction = Vector3::SafeNormalize(Vector3(rotation[2][0], rotation[2][1], rotation[2][2]));
			desc.info.colour = light.colour;
			desc.info.attenuation = light.attenuation;
			desc.info.intensity = light.intensity;
			desc.info.range = light.range;
			desc.info.cone = light.coneFalloff;
			desc.info.castsShadow = light.castsShadow;
			return desc;
		}

		// Replaces a light component with an edited copy, keeping the renderer's handle for it.
		template<typename T>
		T& ReplaceLight(EntitySystem& entities, Entity entity, const void* data)
		{
			T& light = entities.GetComponent<T>(entity);
			const decltype(light.lightHandle) lightHandle = light.lightHandle;
			light = *static_cast<const T*>(data);
			light.lightHandle = lightHandle;
			return light;
		}
	}

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
		DebugDraw::Flush(m_ActiveWorld, deltaTime, *m_RendererAPI);
	}

	void WorldManager::UpdateWorld(Handle worldHandle, World& world, float deltaTime)
	{
		// Creates renderer-side mesh instances and lights for any components added since last time.
		if (world.syncedEntitiesVersion != world.entities.GetVersion())
		{
			world.syncedEntitiesVersion = world.entities.GetVersion();

			// A mesh component may have no mesh yet, such as on a newly placed actor.
			world.entities.ForEach<MeshComponent>([this, worldHandle](Entity entity, MeshComponent& meshComponent)
			{
				if (!meshComponent.meshInstanceRequested && AssetUtil::IsValidAssetID(meshComponent.mesh))
				{
					SyncMeshInstance(worldHandle, entity, meshComponent);
				}
			});

			world.entities.ForEach<DirLightComponent>([&world, this](Entity /*entity*/, DirLightComponent& light)
			{
				if (!light.lightHandle)
				{
					light.lightHandle = m_RendererAPI->CreateDirectionalLight(world.sceneHandle, CreateLightDesc(light));
				}
			});

			world.entities.ForEach<PointLightComponent>([&world, this](Entity entity, PointLightComponent& light)
			{
				if (!light.lightHandle)
				{
					light.lightHandle = m_RendererAPI->CreatePointLight(world.sceneHandle, CreateLightDesc(light, GetWorldTransform(world.entities, entity)));
				}
			});

			world.entities.ForEach<SpotLightComponent>([&world, this](Entity entity, SpotLightComponent& light)
			{
				if (!light.lightHandle)
				{
					light.lightHandle = m_RendererAPI->CreateSpotLight(world.sceneHandle, CreateLightDesc(light, GetWorldTransform(world.entities, entity)));
				}
			});
		}

		// TODO: Step physics and audio here once their components exist, only when world.simulate
		// is set - a world open in the level editor must stay still and silent.

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

		m_RendererAPI->SetSceneAmbient(world.sceneHandle, world.settings.ambient);

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
		world.simulate = config.simulate;
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
		DebugDraw::SetWorld(worldHandle);

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
		DebugDraw::ClearWorld(worldHandle);
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
			DebugDraw::ClearWorld(worldHandle);
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

				World& world = m_WorldPool[worldHandle];
				MeshComponent& comp = world.entities.GetComponent<MeshComponent>(entity);
				comp.meshInstance = handle;
				comp.loadedMaterials = materials;

				// An empty box, such as on a newly placed actor, takes the mesh's bounds.
				if (world.entities.HasComponent<BoxComponent>(entity))
				{
					BoxComponent& box = world.entities.GetComponent<BoxComponent>(entity);
					const MeshHeader* header = m_AssetManager->GetMeshHeader(meshID);
					if (header && box.halfExtents == Vector3::c_Zero)
					{
						box.center = (header->aabbMin + header->aabbMax) * 0.5f;
						box.halfExtents = (header->aabbMax - header->aabbMin) * 0.5f;
					}
				}

				// The entity may have been moved while its mesh was loading.
				UpdateEntityTransform(world, entity);
			});
	}

	bool WorldManager::HasRoomForActorLights(Handle worldHandle, const ActorTypeDesc& actorType)
	{
		EntitySystem& entities = m_WorldPool[worldHandle].entities;
		return HasRoomFor<DirLightComponent>(entities, actorType, RenderConstants::c_MaxDirLights)
			&& HasRoomFor<PointLightComponent>(entities, actorType, RenderConstants::c_MaxPointLights)
			&& HasRoomFor<SpotLightComponent>(entities, actorType, RenderConstants::c_MaxSpotLights);
	}

	bool WorldManager::SetComponentData(Handle worldHandle, Entity entity, ComponentTypeID typeID, const void* data)
	{
		World& world = m_WorldPool[worldHandle];
		++world.changeCount;

		if (typeID == ComponentRegistry::GetComponentTypeID<MeshComponent>())
		{
			return SetMeshComponent(worldHandle, entity, *static_cast<const MeshComponent*>(data));
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<DirLightComponent>())
		{
			DirLightComponent& light = ReplaceLight<DirLightComponent>(world.entities, entity, data);
			light.direction = Vector3::SafeNormalize(light.direction);
			if (light.lightHandle)
			{
				m_RendererAPI->UpdateDirectionalLight(light.lightHandle, CreateLightDesc(light));
			}
			return true;
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<PointLightComponent>())
		{
			const PointLightComponent& light = ReplaceLight<PointLightComponent>(world.entities, entity, data);
			if (light.lightHandle)
			{
				m_RendererAPI->UpdatePointLight(light.lightHandle, CreateLightDesc(light, GetWorldTransform(world.entities, entity)));
			}
			return true;
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<SpotLightComponent>())
		{
			const SpotLightComponent& light = ReplaceLight<SpotLightComponent>(world.entities, entity, data);
			if (light.lightHandle)
			{
				m_RendererAPI->UpdateSpotLight(light.lightHandle, CreateLightDesc(light, GetWorldTransform(world.entities, entity)));
			}
			return true;
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<ComponentTransform>())
		{
			world.entities.GetComponent<ComponentTransform>(entity) = *static_cast<const ComponentTransform*>(data);

			// Moving the actor's root carries the rest of the actor with it.
			for (const ActorInstance& instance : world.actorInstances)
			{
				for (Entity actorEntity : instance.entities)
				{
					if (actorEntity == entity)
					{
						const Entity rootEntity = instance.RootEntity();
						SetActorTransform(worldHandle, rootEntity, world.entities.GetComponent<ComponentTransform>(rootEntity).local);
						return true;
					}
				}
			}
			return true;
		}

		// Anything else is plain data the world doesn't act on.
		const size_t size = TypeRegistry::Instance().GetType(ComponentRegistry::Instance().GetReflectionTypeID(typeID)).size;
		memcpy(world.entities.GetComponentData(entity, typeID), data, size);
		return true;
	}

	bool WorldManager::SetMeshComponent(Handle worldHandle, Entity entity, const MeshComponent& meshComponent)
	{
		World& world = m_WorldPool[worldHandle];
		MeshComponent& current = world.entities.GetComponent<MeshComponent>(entity);

		// Its instance can only be swapped for a new one once it exists.
		if (current.meshInstanceRequested && !current.meshInstance)
		{
			return false;
		}

		bool materialsChanged = current.materials.Size() != meshComponent.materials.Size();
		for (uint i = 0; !materialsChanged && i < current.materials.Size(); ++i)
		{
			materialsChanged = !(current.materials[i] == meshComponent.materials[i]);
		}
		const bool meshChanged = !(current.mesh == meshComponent.mesh);
		if (!meshChanged && !materialsChanged)
		{
			return true;
		}

		// Only meshes and materials can go in these fields.
		const AssetRegistry& registry = AssetRegistry::Instance();
		if (AssetUtil::IsValidAssetID(meshComponent.mesh) && !HasExtension(registry.GetAssetData(meshComponent.mesh).filePath.CStr(), AssetConstants::c_MeshFileExtension))
		{
			TYR_LOG_WARNING("Only a mesh can be used as a mesh component's mesh.");
			return false;
		}
		for (AssetID materialID : meshComponent.materials)
		{
			if (AssetUtil::IsValidAssetID(materialID) && !HasExtension(registry.GetAssetData(materialID).filePath.CStr(), AssetConstants::c_MaterialFileExtension))
			{
				TYR_LOG_WARNING("Only materials can be used as a mesh component's materials.");
				return false;
			}
		}

		RemoveMeshInstance(current);
		current.mesh = meshComponent.mesh;
		// A different mesh starts from its own materials and bounds.
		if (meshChanged)
		{
			current.materials.Clear();
			if (world.entities.HasComponent<BoxComponent>(entity))
			{
				world.entities.GetComponent<BoxComponent>(entity).halfExtents = Vector3::c_Zero;
			}
		}
		else
		{
			current.materials = meshComponent.materials;
		}
		current.loadedMaterials.Clear();
		current.meshInstance = {};
		current.meshInstanceRequested = false;

		if (AssetUtil::IsValidAssetID(current.mesh))
		{
			SyncMeshInstance(worldHandle, entity, current);
		}
		return true;
	}

	void WorldManager::UpdateEntityTransform(World& world, Entity entity)
	{
		if (!world.entities.HasComponent<ComponentTransform>(entity))
		{
			return;
		}
		const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;

		if (world.entities.HasComponent<MeshComponent>(entity))
		{
			const MeshComponent& meshComponent = world.entities.GetComponent<MeshComponent>(entity);
			if (meshComponent.meshInstance)
			{
				MeshInstanceDesc desc;
				desc.info.transform = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
				desc.info.mesh = m_AssetManager->GetMesh(meshComponent.mesh);
				for (AssetID materialID : meshComponent.loadedMaterials)
				{
					desc.info.materials.Add(m_AssetManager->GetMaterial(materialID));
				}
				m_RendererAPI->UpdateMeshInstance(meshComponent.meshInstance, desc);
			}
		}

		if (world.entities.HasComponent<PointLightComponent>(entity))
		{
			const PointLightComponent& light = world.entities.GetComponent<PointLightComponent>(entity);
			if (light.lightHandle)
			{
				m_RendererAPI->UpdatePointLight(light.lightHandle, CreateLightDesc(light, transform));
			}
		}

		if (world.entities.HasComponent<SpotLightComponent>(entity))
		{
			const SpotLightComponent& light = world.entities.GetComponent<SpotLightComponent>(entity);
			if (light.lightHandle)
			{
				m_RendererAPI->UpdateSpotLight(light.lightHandle, CreateLightDesc(light, transform));
			}
		}
	}

	void WorldManager::SetActorTransform(Handle worldHandle, Entity rootEntity, const Transform& transform)
	{
		World& world = m_WorldPool[worldHandle];
		for (const ActorInstance& instance : world.actorInstances)
		{
			if (instance.RootEntity() != rootEntity)
			{
				continue;
			}

			++world.changeCount;

			// The root has no parent, so its local and world transforms are the same.
			ComponentTransform& root = world.entities.GetComponent<ComponentTransform>(rootEntity);
			root.local = transform;
			root.world = transform;
			UpdateEntityTransform(world, rootEntity);

			// Parents come before their children in the actor's entity list.
			for (uint i = 1; i < instance.entities.Size(); ++i)
			{
				const Entity entity = instance.entities[i];
				ComponentTransform& child = world.entities.GetComponent<ComponentTransform>(entity);
				if (child.parentEntity == c_InvalidEntity)
				{
					continue;
				}

				const Transform& parentWorld = world.entities.GetComponent<ComponentTransform>(child.parentEntity).world;
				const Matrix4 worldMatrix = Matrix4::CreateTRS(child.local.position, child.local.rotation, child.local.scale)
					* Matrix4::CreateTRS(parentWorld.position, parentWorld.rotation, parentWorld.scale);
				worldMatrix.DecomposeTRS(child.world.position, child.world.rotation, child.world.scale);
				UpdateEntityTransform(world, entity);
			}
			return;
		}
	}

	void WorldManager::RemoveMeshInstance(const MeshComponent& meshComponent)
	{
		// One that's still loading is released by its creation callback once it finishes.
		if (meshComponent.meshInstance)
		{
			ReleaseMeshInstance(meshComponent.meshInstance, meshComponent.mesh, meshComponent.loadedMaterials);
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

	Id64 WorldManager::CreateActorInstanceID()
	{
		Guid guid;
		Platform::CreateGuid(guid);
		return Id64(guid);
	}

	void WorldManager::AddActorInstance(Handle worldHandle, const Id64& id, const Id64& typeID, const char* name, const char* folderPath, const LocalArray<Entity, c_MaxActorInstanceEntities>& entities)
	{
		World& world = m_WorldPool[worldHandle];

		ActorInstance& instance = world.actorInstances.ExpandOne();
		instance.id = id;
		instance.typeID = typeID;
		instance.name = name;
		instance.folderPath = folderPath;
		instance.entities = entities;
		++world.changeCount;
	}

	void WorldManager::ClearWorld(Handle worldHandle)
	{
		World& world = m_WorldPool[worldHandle];
		while (!world.actorInstances.IsEmpty())
		{
			RemoveActorInstance(worldHandle, world.actorInstances.Back().RootEntity());
		}
		world.folders.Clear();
		world.settings = {};
		++world.changeCount;
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
				if (world.entities.HasComponent<DirLightComponent>(entity))
				{
					ReleaseLight(world, world.entities.GetComponent<DirLightComponent>(entity));
				}
				if (world.entities.HasComponent<PointLightComponent>(entity))
				{
					ReleaseLight(world, world.entities.GetComponent<PointLightComponent>(entity));
				}
				if (world.entities.HasComponent<SpotLightComponent>(entity))
				{
					ReleaseLight(world, world.entities.GetComponent<SpotLightComponent>(entity));
				}
				world.entities.RemoveEntity(entity);
			}

			world.actorInstances.SwapAndPopBack(i);
			++world.changeCount;
			return;
		}
	}

	void WorldManager::ShutdownWorld(World& world)
	{
		world.entities.ForEach<MeshComponent>([this](Entity /*entity*/, MeshComponent& meshComponent)
		{
			RemoveMeshInstance(meshComponent);
		});

		world.entities.ForEach<DirLightComponent>([this, &world](Entity /*entity*/, DirLightComponent& light)
		{
			ReleaseLight(world, light);
		});
		world.entities.ForEach<PointLightComponent>([this, &world](Entity /*entity*/, PointLightComponent& light)
		{
			ReleaseLight(world, light);
		});
		world.entities.ForEach<SpotLightComponent>([this, &world](Entity /*entity*/, SpotLightComponent& light)
		{
			ReleaseLight(world, light);
		});

		m_RendererAPI->RemoveScene(world.sceneHandle);
		m_RendererAPI->DeleteRenderViewport(world.renderViewportHandle);
	}

	void WorldManager::ReleaseLight(const World& world, DirLightComponent& light)
	{
		if (light.lightHandle)
		{
			m_RendererAPI->DeleteDirectionalLight(world.sceneHandle, light.lightHandle);
			light.lightHandle = {};
		}
	}

	void WorldManager::ReleaseLight(const World& world, PointLightComponent& light)
	{
		if (light.lightHandle)
		{
			m_RendererAPI->DeletePointLight(world.sceneHandle, light.lightHandle);
			light.lightHandle = {};
		}
	}

	void WorldManager::ReleaseLight(const World& world, SpotLightComponent& light)
	{
		if (light.lightHandle)
		{
			m_RendererAPI->DeleteSpotLight(world.sceneHandle, light.lightHandle);
			light.lightHandle = {};
		}
	}
}
