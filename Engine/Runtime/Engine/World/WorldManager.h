#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "World.h"
#include "RenderInstance/RenderInstances.h"
#include "Animation/AnimationSystem.h"
#include "Physics/PhysicsManager.h"

namespace tyr
{
	class RendererAPI;
	class WindowModule;
	class AssetManager;
	struct MeshComponent;
	struct SkeletalMeshComponent;
	struct AnimatorComponent;
	struct DirLightComponent;
	struct PointLightComponent;
	struct SpotLightComponent;
	struct DecalComponent;
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

		// A new, permanent ID for an actor being placed.
		static Id64 CreateActorInstanceID();

		// Adds an actor of the type whose entities have already been created in the world.
		void AddActorInstance(Handle worldHandle, const Id64& id, const Id64& typeID, const char* name, const char* folderPath, const LocalArray<Entity, c_MaxActorInstanceEntities>& entities);

		// Removes the actor and all of its entities.
		void RemoveActorInstance(Handle worldHandle, Entity rootEntity);

		// Removes every actor and folder, and resets the level settings, ready for another level.
		void ClearWorld(Handle worldHandle);

		// Moves the actor's root, carrying its child entities and their mesh instances with it.
		void SetActorTransform(Handle worldHandle, Entity rootEntity, const Transform& transform);

		// False when adding an actor of this type would take the world over the renderer's limit
		// for any type of light, or for decals.
		bool HasRoomForActorLights(Handle worldHandle, const ActorTypeDesc& actorType);

		// Replaces one of the entity's components, such as after editing it, and updates whatever
		// the world made from it, like its mesh instance or light. Returns false if it can't be
		// changed right now.
		bool SetComponentData(Handle worldHandle, Entity entity, ComponentTypeID typeID, const void* data);

		// Plays animator clips, such as cross-fading to another with AnimationSystem::Play.
		AnimationSystem& GetAnimationSystem() { return m_AnimationSystem; }

		// Raycasts and other queries against a simulated world's physics.
		PhysicsManager& GetPhysicsManager() { return m_PhysicsManager; }

	private:
		// Only called from AddWorld/RemoveWorld(s), which keep the pool and m_Worlds in step.
		void InitWorld(World& world, const WorldConfig& config);
		void ShutdownWorld(World& world);

		// Requests the mesh instance for the component and stores it on the component once created.
		void SyncMeshInstance(Handle worldHandle, Entity entity, MeshComponent& meshComponent);
		// Releases the component's mesh instance along with its mesh and materials.
		void RemoveMeshInstance(const MeshComponent& meshComponent);
		void ReleaseMeshInstance(MeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials);
		// The same for a skinned mesh.
		void SyncSkeletalMeshInstance(Handle worldHandle, Entity entity, SkeletalMeshComponent& skeletalMesh);
		void RemoveSkeletalMeshInstance(const SkeletalMeshComponent& skeletalMesh);
		void ReleaseSkeletalMeshInstance(SkeletalMeshInstanceHandle handle, AssetID meshID, const LocalArray<AssetID, MeshConstants::c_MaxSubmeshes>& materials);
		bool SetSkeletalMeshComponent(Handle worldHandle, Entity entity, const SkeletalMeshComponent& skeletalMesh);
		// Only animation clips made for the entity's skeleton can go in its animator.
		bool SetAnimatorComponent(Handle worldHandle, Entity entity, const AnimatorComponent& animator);
		// Releases everything the world made from the entity's components, before it's removed.
		void ReleaseEntity(World& world, Entity entity);
		// Empty bounds and box colliders, such as on a newly placed actor, take the mesh's bounds.
		void FitBoundsToMesh(World& world, Entity entity, AssetID meshID);
		// Clears the bounds and box collider sized from a mesh, so they're fitted to a new one.
		void ResetMeshBounds(World& world, Entity entity);
		// Holds the collision mesh of the entity's mesh while its collider is a mesh or convex hull,
		// once the mesh has loaded. Anything else lets go of it.
		void SyncColliderMesh(World& world, Entity entity);
		void ReleaseColliderMesh(World& world, ColliderComponent& collider);
		// Moves entities with dynamic bodies to where physics put them, blended between steps.
		void ApplyPhysicsTransforms(World& world);
		// Moves an entity in world space, carrying the actor's entities under it along.
		void MoveEntity(World& world, Entity entity, const Vector3& position, const Quaternion& rotation);
		// Recomputes the world transforms of the actor's entities under the moved one.
		void PropagateActorTransforms(World& world, const ActorInstance& instance, Entity moved);
		// Sends the entity's world transform to its mesh instance and lights, if it has them yet.
		void UpdateEntityTransform(World& world, Entity entity);
		// Removes the light from the renderer, if it was ever added.
		void ReleaseLight(const World& world, DirLightComponent& light);
		void ReleaseLight(const World& world, PointLightComponent& light);
		void ReleaseLight(const World& world, SpotLightComponent& light);
		// Makes the renderer's decals once their materials have loaded.
		void SyncDecals(World& world);
		// Removes the decal from the renderer and lets go of its material.
		void ReleaseDecal(const World& world, DecalComponent& decal);
		// Swaps the mesh instance for one using the new mesh and materials. Refused while the
		// current one is still loading.
		bool SetMeshComponent(Handle worldHandle, Entity entity, const MeshComponent& meshComponent);

		RendererAPI* m_RendererAPI;
		WindowModule* m_WindowModule;
		AssetManager* m_AssetManager;
		AnimationSystem m_AnimationSystem;
		PhysicsManager m_PhysicsManager;
		LocalObjectPool<World, c_MaxWorlds, ResetObjectPolicy> m_WorldPool;
		LocalArray<Handle, c_MaxWorlds> m_Worlds;
		Handle m_ActiveWorld;
	};
	
}