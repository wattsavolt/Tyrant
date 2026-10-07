#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "World.h"
#include "RenderInstance/RenderInstances.h"

namespace tyr
{
	class RendererAPI;
	class WindowModule;
	class AssetManager;
	struct MeshComponent;
	struct DirLightComponent;
	struct PointLightComponent;
	struct SpotLightComponent;
	struct Transform;
	struct ActorTypeDesc;
	class TYR_ENGINE_API WorldManager final
	{
	public:
		static constexpr uint8 c_MaxWorlds = RenderConstants::c_MaxScenes;

		WorldManager();
		~WorldManager();

		void Update(float deltaTime);

		void UpdateWorld(Handle worldHandle, World& world, float deltaTime);

		Handle AddWorld(const WorldConfig& params);

		const World& GetWorld(Handle worldHandle) const;

		World& GetWorld(Handle worldHandle);

		void RemoveWorld(Handle worldHandle);

		void RemoveWorlds();

		// Only one world's scene ever renders. Switching which one does is the only place
		// SetSceneWindow gets called - it's a no-op if worldHandle is already the active world.
		// Pass an empty handle to stop any world from rendering.
		void SetActiveWorld(Handle worldHandle);

		Handle GetActiveWorld() const { return m_ActiveWorld; }

		// Updates the world's cached render window. If the world is currently active,
		// this also pushes the change to RendererAPI immediately via SetSceneWindow.
		void SetWorldWindow(Handle worldHandle, RenderWindowHandle windowHandle);

		// Adds an actor whose entities have already been created in the world.
		void AddActorInstance(Handle worldHandle, const char* name, const char* folderPath, const LocalArray<Entity, c_MaxActorInstanceEntities>& entities);

		// Removes the actor and all of its entities.
		void RemoveActorInstance(Handle worldHandle, Entity rootEntity);

		// Moves the actor's root, carrying its child entities and their mesh instances with it.
		void SetActorTransform(Handle worldHandle, Entity rootEntity, const Transform& transform);

		// False when adding an actor of this type would take the world over the renderer's limit
		// for any type of light.
		bool HasRoomForActorLights(Handle worldHandle, const ActorTypeDesc& actorType);

		// Replaces one of the entity's components, such as after editing it, and updates whatever
		// the world made from it, like its mesh instance or light. Returns false if it can't be
		// changed right now.
		bool SetComponentData(Handle worldHandle, Entity entity, ComponentTypeID typeID, const void* data);

	private:
		// Only called from AddWorld/RemoveWorld(s), which keep the pool and m_Worlds in step.
		void InitWorld(World& world, const WorldConfig& config);
		void ShutdownWorld(World& world);

		// Requests the mesh instance for the component and stores it on the component once created.
		void SyncMeshInstance(Handle worldHandle, Entity entity, MeshComponent& meshComponent);
		// Releases the component's mesh instance along with its mesh and materials.
		void RemoveMeshInstance(const MeshComponent& meshComponent);
		void ReleaseMeshInstance(MeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials);
		// Sends the entity's world transform to its mesh instance and lights, if it has them yet.
		void UpdateEntityTransform(World& world, Entity entity);
		// Removes the light from the renderer, if it was ever added.
		void ReleaseLight(const World& world, DirLightComponent& light);
		void ReleaseLight(const World& world, PointLightComponent& light);
		void ReleaseLight(const World& world, SpotLightComponent& light);
		// Swaps the mesh instance for one using the new mesh and materials. Refused while the
		// current one is still loading.
		bool SetMeshComponent(Handle worldHandle, Entity entity, const MeshComponent& meshComponent);

		RendererAPI* m_RendererAPI;
		WindowModule* m_WindowModule;
		AssetManager* m_AssetManager;
		LocalObjectPool<World, c_MaxWorlds, ResetObjectPolicy> m_WorldPool;
		LocalArray<Handle, c_MaxWorlds> m_Worlds;
		Handle m_ActiveWorld;
	};
	
}