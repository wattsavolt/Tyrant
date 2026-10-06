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

	private:
		// Only called from AddWorld/RemoveWorld(s), which keep the pool and m_Worlds in step.
		void InitWorld(World& world, const WorldConfig& config);
		void ShutdownWorld(World& world);

		// Requests the mesh instance for the component and stores it on the component once created.
		void SyncMeshInstance(Handle worldHandle, Entity entity, MeshComponent& meshComponent);
		// Releases the component's mesh instance along with its mesh and materials.
		void RemoveMeshInstance(const MeshComponent& meshComponent);
		void ReleaseMeshInstance(MeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials);

		RendererAPI* m_RendererAPI;
		WindowModule* m_WindowModule;
		AssetManager* m_AssetManager;
		LocalObjectPool<World, c_MaxWorlds, ResetObjectPolicy> m_WorldPool;
		LocalArray<Handle, c_MaxWorlds> m_Worlds;
		Handle m_ActiveWorld;
	};
	
}