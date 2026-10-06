#include "ActorUtil.h"

namespace tyr
{
	namespace
	{
		Entity CreateMeshEntity(EntitySystem& entities, const StaticMeshActorMeshDesc& desc, Entity parent)
		{
			const Entity entity = entities.CreateEntity();

			ComponentTransform transform;
			transform.local = desc.localTransform;
			transform.world = desc.localTransform;
			transform.parentEntity = parent;
			entities.AddComponent<ComponentTransform>(entity, transform);

			MeshComponent meshComponent;
			// Materials left empty so the mesh's own materials are used.
			meshComponent.mesh = desc.mesh;
			entities.AddComponent<MeshComponent>(entity, meshComponent);

			BoxComponent boxComponent;
			boxComponent.center = (desc.aabbMin + desc.aabbMax) * 0.5f;
			boxComponent.halfExtents = (desc.aabbMax - desc.aabbMin) * 0.5f;
			entities.AddComponent<BoxComponent>(entity, boxComponent);

			return entity;
		}
	}

	Entity ActorUtil::BuildStaticMeshActor(EntitySystem& entities, const StaticMeshActorMeshDesc& mesh)
	{
		return BuildStaticMeshesActor(entities, &mesh, 1)[0];
	}

	LocalArray<Entity, c_MaxStaticMeshActorEntities> ActorUtil::BuildStaticMeshesActor(EntitySystem& entities, const StaticMeshActorMeshDesc* meshes, uint count)
	{
		TYR_ASSERT(count > 0 && count <= c_MaxStaticMeshActorMeshes);

		LocalArray<Entity, c_MaxStaticMeshActorEntities> result;

		if (count == 1)
		{
			result.Add(CreateMeshEntity(entities, meshes[0], c_InvalidEntity));
			return result;
		}

		const Entity root = entities.CreateEntity();

		ComponentTransform rootTransform;
		rootTransform.local.scale = Vector3::c_One;
		rootTransform.local.rotation = Quaternion::c_Identity;
		rootTransform.local.position = Vector3::c_Zero;
		rootTransform.world = rootTransform.local;
		rootTransform.parentEntity = c_InvalidEntity;
		entities.AddComponent<ComponentTransform>(root, rootTransform);
		result.Add(root);

		for (uint i = 0; i < count; ++i)
		{
			result.Add(CreateMeshEntity(entities, meshes[i], root));
		}

		return result;
	}
}
