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
		m_AnimationSystem.Initialize(*m_AssetManager, *m_RendererAPI);
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

			world.entities.ForEach<SkeletalMeshComponent>([this, worldHandle](Entity entity, SkeletalMeshComponent& skeletalMesh)
			{
				if (!skeletalMesh.meshInstanceRequested && AssetUtil::IsValidAssetID(skeletalMesh.mesh))
				{
					SyncSkeletalMeshInstance(worldHandle, entity, skeletalMesh);
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

		if (world.physicsWorld && !world.paused)
		{
			m_PhysicsManager.Update(world.physicsWorld, world, deltaTime);
			ApplyPhysicsTransforms(world);
		}

		if (!world.paused)
		{
			m_AnimationSystem.Update(world, deltaTime);
		}

		// The other way a resize can be detected - externally, by the OS/windowing system -
		// rather than internally by the renderer itself. Always consumed, but only acted on if
		// the window is still active, since a resize can fire as part of the window being destroyed.
		const bool resizePending = world.windowHandle && m_WindowModule->ConsumeResizePending(world.windowHandle);
		if (resizePending && m_WindowModule->IsWindowActive(world.windowHandle))
		{
			m_RendererAPI->ResizeWindow(world.renderWindowHandle);
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
		world.renderWindowHandle = config.windowHandle;
		world.windowHandle = config.osWindowHandle;
		if (world.simulate)
		{
			world.physicsWorld = m_PhysicsManager.CreatePhysicsWorld();
		}

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
			m_RendererAPI->SetSceneWindow(world.sceneHandle, world.renderWindowHandle);
			m_RendererAPI->SetSceneRenderViewport(world.sceneHandle, world.renderViewportHandle);
		}
	}

	void WorldManager::SetWorldWindow(Handle worldHandle, RenderWindowHandle windowHandle)
	{
		World& world = m_WorldPool[worldHandle];
		world.renderWindowHandle = windowHandle;

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
		m_AssetManager->LoadMeshInstanceAssets(meshID, meshComponent.materials, false,
			[this, worldHandle, entity, meshID](const MeshInstanceAssets& assets)
			{
				// The entity or its world was removed while this was loading, so nothing owns it.
				if (!m_WorldPool.IsValid(worldHandle) || !m_WorldPool[worldHandle].entities.HasComponent<MeshComponent>(entity))
				{
					ReleaseMeshInstance({}, meshID, assets.materialIDs);
					return;
				}

				// Created where the entity is now, which may have changed while it was loading.
				World& world = m_WorldPool[worldHandle];
				const Transform transform = GetWorldTransform(world.entities, entity);
				MeshInstanceDesc desc;
				desc.info.transform = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
				desc.info.mesh = assets.mesh;
				desc.info.materials = assets.materials;

				MeshComponent& comp = world.entities.GetComponent<MeshComponent>(entity);
				comp.meshInstance = m_RendererAPI->CreateMeshInstance(world.sceneHandle, desc);
				comp.loadedMaterials = assets.materialIDs;
				FitBoundsToMesh(world, entity, meshID);
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

		if (typeID == ComponentRegistry::GetComponentTypeID<SkeletalMeshComponent>())
		{
			return SetSkeletalMeshComponent(worldHandle, entity, *static_cast<const SkeletalMeshComponent*>(data));
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<AnimatorComponent>())
		{
			return SetAnimatorComponent(worldHandle, entity, *static_cast<const AnimatorComponent*>(data));
		}

		// A changed collider or rigid body gets a new body on the next physics update.
		if (typeID == ComponentRegistry::GetComponentTypeID<ColliderComponent>())
		{
			ColliderComponent& collider = world.entities.GetComponent<ColliderComponent>(entity);
			if (world.physicsWorld)
			{
				m_PhysicsManager.RemoveBody(world.physicsWorld, collider);
			}
			collider = *static_cast<const ColliderComponent*>(data);
			collider.bodyID = PhysicsConstants::c_InvalidBodyID;
			return true;
		}

		if (typeID == ComponentRegistry::GetComponentTypeID<RigidBodyComponent>())
		{
			world.entities.GetComponent<RigidBodyComponent>(entity) = *static_cast<const RigidBodyComponent*>(data);
			if (world.physicsWorld && world.entities.HasComponent<ColliderComponent>(entity))
			{
				m_PhysicsManager.RemoveBody(world.physicsWorld, world.entities.GetComponent<ColliderComponent>(entity));
			}
			return true;
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
		if (AssetUtil::IsValidAssetID(meshComponent.mesh) && !AssetUtil::HasExtension(registry.GetAssetData(meshComponent.mesh).filePath.CStr(), AssetConstants::c_MeshFileExtension))
		{
			TYR_LOG_WARNING("Only a mesh can be used as a mesh component's mesh.");
			return false;
		}
		for (AssetID materialID : meshComponent.materials)
		{
			if (AssetUtil::IsValidAssetID(materialID) && !AssetUtil::HasExtension(registry.GetAssetData(materialID).filePath.CStr(), AssetConstants::c_MaterialFileExtension))
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
			ResetMeshBounds(world, entity);
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

		if (world.entities.HasComponent<SkeletalMeshComponent>(entity))
		{
			const SkeletalMeshComponent& skeletalMesh = world.entities.GetComponent<SkeletalMeshComponent>(entity);
			if (skeletalMesh.meshInstance)
			{
				SkeletalMeshInstanceDesc desc;
				desc.info.transform = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
				desc.info.mesh = m_AssetManager->GetMesh(skeletalMesh.mesh);
				for (AssetID materialID : skeletalMesh.loadedMaterials)
				{
					desc.info.materials.Add(m_AssetManager->GetMaterial(materialID));
				}
				m_RendererAPI->UpdateSkeletalMeshInstance(skeletalMesh.meshInstance, desc);
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
			PropagateActorTransforms(world, instance, rootEntity);
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
		if (handle)
		{
			m_RendererAPI->DeleteMeshInstance(handle);
		}
		for (AssetID materialID : materials)
		{
			m_AssetManager->DeleteMaterial(materialID);
		}
		m_AssetManager->DeleteMesh(meshID);
	}

	void WorldManager::SyncSkeletalMeshInstance(Handle worldHandle, Entity entity, SkeletalMeshComponent& skeletalMesh)
	{
		skeletalMesh.meshInstanceRequested = true;

		const AssetID meshID = skeletalMesh.mesh;
		m_AssetManager->LoadMeshInstanceAssets(meshID, skeletalMesh.materials, true,
			[this, worldHandle, entity, meshID](const MeshInstanceAssets& assets)
			{
				// The entity or its world was removed while this was loading, so nothing owns it.
				if (!m_WorldPool.IsValid(worldHandle) || !m_WorldPool[worldHandle].entities.HasComponent<SkeletalMeshComponent>(entity))
				{
					ReleaseSkeletalMeshInstance({}, meshID, assets.materialIDs);
					return;
				}

				// Created where the entity is now, which may have changed while it was loading.
				World& world = m_WorldPool[worldHandle];
				const Transform transform = GetWorldTransform(world.entities, entity);
				SkeletalMeshInstanceDesc desc;
				desc.info.transform = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
				desc.info.mesh = assets.mesh;
				desc.info.materials = assets.materials;

				SkeletalMeshComponent& comp = world.entities.GetComponent<SkeletalMeshComponent>(entity);
				comp.meshInstance = m_RendererAPI->CreateSkeletalMeshInstance(world.sceneHandle, desc);
				comp.loadedMaterials = assets.materialIDs;
				FitBoundsToMesh(world, entity, meshID);
			});
	}

	void WorldManager::RemoveSkeletalMeshInstance(const SkeletalMeshComponent& skeletalMesh)
	{
		// One that's still loading is released by its creation callback once it finishes.
		if (skeletalMesh.meshInstance)
		{
			ReleaseSkeletalMeshInstance(skeletalMesh.meshInstance, skeletalMesh.mesh, skeletalMesh.loadedMaterials);
		}
	}

	void WorldManager::ReleaseSkeletalMeshInstance(SkeletalMeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials)
	{
		if (handle)
		{
			m_RendererAPI->DeleteSkeletalMeshInstance(handle);
		}
		for (AssetID materialID : materials)
		{
			m_AssetManager->DeleteMaterial(materialID);
		}
		m_AssetManager->DeleteMesh(meshID);
	}

	bool WorldManager::SetSkeletalMeshComponent(Handle worldHandle, Entity entity, const SkeletalMeshComponent& skeletalMesh)
	{
		World& world = m_WorldPool[worldHandle];
		SkeletalMeshComponent& current = world.entities.GetComponent<SkeletalMeshComponent>(entity);

		// Its instance can only be swapped for a new one once it exists.
		if (current.meshInstanceRequested && !current.meshInstance)
		{
			return false;
		}

		bool materialsChanged = current.materials.Size() != skeletalMesh.materials.Size();
		for (uint i = 0; !materialsChanged && i < current.materials.Size(); ++i)
		{
			materialsChanged = !(current.materials[i] == skeletalMesh.materials[i]);
		}
		const bool meshChanged = !(current.mesh == skeletalMesh.mesh);
		if (!meshChanged && !materialsChanged)
		{
			return true;
		}

		const AssetRegistry& registry = AssetRegistry::Instance();
		if (AssetUtil::IsValidAssetID(skeletalMesh.mesh) && !AssetUtil::HasExtension(registry.GetAssetData(skeletalMesh.mesh).filePath.CStr(), AssetConstants::c_SkeletalMeshFileExtension))
		{
			TYR_LOG_WARNING("Only a skeletal mesh can be used as a skeletal mesh component's mesh.");
			return false;
		}
		for (AssetID materialID : skeletalMesh.materials)
		{
			if (AssetUtil::IsValidAssetID(materialID) && !AssetUtil::HasExtension(registry.GetAssetData(materialID).filePath.CStr(), AssetConstants::c_MaterialFileExtension))
			{
				TYR_LOG_WARNING("Only materials can be used as a skeletal mesh component's materials.");
				return false;
			}
		}

		RemoveSkeletalMeshInstance(current);
		current.mesh = skeletalMesh.mesh;
		// A different mesh starts from its own materials and bounds.
		if (meshChanged)
		{
			current.materials.Clear();
			ResetMeshBounds(world, entity);
		}
		else
		{
			current.materials = skeletalMesh.materials;
		}
		current.loadedMaterials.Clear();
		current.meshInstance = {};
		current.meshInstanceRequested = false;

		if (AssetUtil::IsValidAssetID(current.mesh))
		{
			SyncSkeletalMeshInstance(worldHandle, entity, current);
		}
		return true;
	}

	bool WorldManager::SetAnimatorComponent(Handle worldHandle, Entity entity, const AnimatorComponent& animator)
	{
		World& world = m_WorldPool[worldHandle];

		// The skeleton is known once the entity's skeletal mesh header has loaded.
		const MeshHeader* header = nullptr;
		if (world.entities.HasComponent<SkeletalMeshComponent>(entity))
		{
			header = m_AssetManager->GetMeshHeader(world.entities.GetComponent<SkeletalMeshComponent>(entity).mesh);
		}

		const AssetRegistry& registry = AssetRegistry::Instance();
		const AssetID clips[] = { animator.clip, animator.layerClip };
		for (AssetID clip : clips)
		{
			if (!AssetUtil::IsValidAssetID(clip))
			{
				continue;
			}
			if (!AssetUtil::HasExtension(registry.GetAssetData(clip).filePath.CStr(), AssetConstants::c_AnimationFileExtension))
			{
				TYR_LOG_WARNING("Only animation clips can be played by an animator.");
				return false;
			}

			// A clip depends on the skeleton it was made for.
			uint dependencyCount = 0;
			const AssetID* dependencies = registry.GetAssetDependencies(clip, dependencyCount);
			if (header && (dependencyCount == 0 || !(dependencies[0] == header->skeleton)))
			{
				TYR_LOG_WARNING("That animation clip was made for a different skeleton.");
				return false;
			}
		}

		// The playing state carries on, and picks the new clips up on its next update.
		AnimatorComponent& current = world.entities.GetComponent<AnimatorComponent>(entity);
		current.clip = animator.clip;
		current.fadeTime = animator.fadeTime;
		current.playRate = animator.playRate;
		current.loop = animator.loop;
		current.layerClip = animator.layerClip;
		current.layerMaskBone = animator.layerMaskBone;
		current.layerWeight = animator.layerWeight;
		return true;
	}

	void WorldManager::ReleaseEntity(World& world, Entity entity)
	{
		if (world.entities.HasComponent<MeshComponent>(entity))
		{
			RemoveMeshInstance(world.entities.GetComponent<MeshComponent>(entity));
		}
		if (world.entities.HasComponent<SkeletalMeshComponent>(entity))
		{
			RemoveSkeletalMeshInstance(world.entities.GetComponent<SkeletalMeshComponent>(entity));
		}
		if (world.entities.HasComponent<AnimatorComponent>(entity))
		{
			m_AnimationSystem.Release(world.entities.GetComponent<AnimatorComponent>(entity));
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
		if (world.physicsWorld && world.entities.HasComponent<ColliderComponent>(entity))
		{
			m_PhysicsManager.RemoveBody(world.physicsWorld, world.entities.GetComponent<ColliderComponent>(entity));
		}
	}

	void WorldManager::FitBoundsToMesh(World& world, Entity entity, AssetID meshID)
	{
		const MeshHeader* header = m_AssetManager->GetMeshHeader(meshID);
		if (!header)
		{
			return;
		}
		const Vector3 center = (header->aabbMin + header->aabbMax) * 0.5f;
		const Vector3 halfExtents = (header->aabbMax - header->aabbMin) * 0.5f;

		if (world.entities.HasComponent<BoxComponent>(entity))
		{
			BoxComponent& box = world.entities.GetComponent<BoxComponent>(entity);
			if (box.halfExtents == Vector3::c_Zero)
			{
				box.center = center;
				box.halfExtents = halfExtents;
			}
		}

		// The physics body is made once the collider has a size.
		if (world.entities.HasComponent<ColliderComponent>(entity))
		{
			ColliderComponent& collider = world.entities.GetComponent<ColliderComponent>(entity);
			if (static_cast<ColliderShape>(collider.shape) == ColliderShape::Box && collider.halfExtents == Vector3::c_Zero)
			{
				collider.center = center;
				collider.halfExtents = halfExtents;
			}
		}
	}

	void WorldManager::ResetMeshBounds(World& world, Entity entity)
	{
		if (world.entities.HasComponent<BoxComponent>(entity))
		{
			world.entities.GetComponent<BoxComponent>(entity).halfExtents = Vector3::c_Zero;
		}
		if (world.entities.HasComponent<ColliderComponent>(entity))
		{
			ColliderComponent& collider = world.entities.GetComponent<ColliderComponent>(entity);
			if (static_cast<ColliderShape>(collider.shape) == ColliderShape::Box)
			{
				collider.halfExtents = Vector3::c_Zero;
				if (world.physicsWorld)
				{
					m_PhysicsManager.RemoveBody(world.physicsWorld, collider);
				}
			}
		}
	}

	void WorldManager::ApplyPhysicsTransforms(World& world)
	{
		const float alpha = m_PhysicsManager.GetInterpolation(world.physicsWorld);
		world.entities.ForEach<RigidBodyComponent>([this, &world, alpha](Entity entity, RigidBodyComponent& rigidBody)
		{
			if (static_cast<MotionType>(rigidBody.motionType) != MotionType::Dynamic || !world.entities.HasComponent<ColliderComponent>(entity)
				|| world.entities.GetComponent<ColliderComponent>(entity).bodyID == PhysicsConstants::c_InvalidBodyID)
			{
				return;
			}

			const Vector3 position = Vector3::Lerp(alpha, rigidBody.previousPosition, rigidBody.currentPosition);
			const Quaternion rotation = Quaternion::Lerp(alpha, rigidBody.previousRotation, rigidBody.currentRotation);

			// A body at rest, such as one asleep, leaves its entity where it already is.
			const Transform& current = world.entities.GetComponent<ComponentTransform>(entity).world;
			if (current.position == position && current.rotation == rotation)
			{
				return;
			}
			MoveEntity(world, entity, position, rotation);
		});
	}

	void WorldManager::MoveEntity(World& world, Entity entity, const Vector3& position, const Quaternion& rotation)
	{
		ComponentTransform& transform = world.entities.GetComponent<ComponentTransform>(entity);
		transform.world.position = position;
		transform.world.rotation = rotation;

		if (transform.parentEntity == c_InvalidEntity)
		{
			transform.local.position = position;
			transform.local.rotation = rotation;
		}
		else
		{
			// The local transform keeps the entity where it is under its parent.
			const Transform& parentWorld = world.entities.GetComponent<ComponentTransform>(transform.parentEntity).world;
			const Matrix4 worldMatrix = Matrix4::CreateTRS(position, rotation, transform.world.scale);
			const Matrix4 localMatrix = worldMatrix * Matrix4::CreateTRS(parentWorld.position, parentWorld.rotation, parentWorld.scale).Inverse();
			localMatrix.DecomposeTRS(transform.local.position, transform.local.rotation, transform.local.scale);
		}
		UpdateEntityTransform(world, entity);

		for (const ActorInstance& instance : world.actorInstances)
		{
			for (Entity actorEntity : instance.entities)
			{
				if (actorEntity == entity)
				{
					PropagateActorTransforms(world, instance, entity);
					return;
				}
			}
		}
	}

	void WorldManager::PropagateActorTransforms(World& world, const ActorInstance& instance, Entity moved)
	{
		LocalArray<Entity, c_MaxActorInstanceEntities> movedEntities;
		movedEntities.Add(moved);

		// Parents come before their children in the actor's entity list.
		for (Entity entity : instance.entities)
		{
			ComponentTransform& child = world.entities.GetComponent<ComponentTransform>(entity);
			bool parentMoved = false;
			for (Entity movedEntity : movedEntities)
			{
				parentMoved |= child.parentEntity == movedEntity;
			}
			if (!parentMoved)
			{
				continue;
			}

			const Transform& parentWorld = world.entities.GetComponent<ComponentTransform>(child.parentEntity).world;
			const Matrix4 worldMatrix = Matrix4::CreateTRS(child.local.position, child.local.rotation, child.local.scale)
				* Matrix4::CreateTRS(parentWorld.position, parentWorld.rotation, parentWorld.scale);
			worldMatrix.DecomposeTRS(child.world.position, child.world.rotation, child.world.scale);
			UpdateEntityTransform(world, entity);
			movedEntities.Add(entity);
		}
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
				ReleaseEntity(world, entity);
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
		world.entities.ForEach<SkeletalMeshComponent>([this](Entity /*entity*/, SkeletalMeshComponent& skeletalMesh)
		{
			RemoveSkeletalMeshInstance(skeletalMesh);
		});
		world.entities.ForEach<AnimatorComponent>([this](Entity /*entity*/, AnimatorComponent& animator)
		{
			m_AnimationSystem.Release(animator);
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

		if (world.physicsWorld)
		{
			m_PhysicsManager.DestroyPhysicsWorld(world.physicsWorld);
			world.physicsWorld = {};
		}

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
