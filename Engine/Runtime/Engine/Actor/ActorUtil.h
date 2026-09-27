#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Containers/LocalArray.h"
#include "ECS/Components.h"

namespace tyr
{
	// The most meshes ActorUtil::BuildStaticMeshesActor takes in one call, plus one more
	// possible entity for the shared root when there's more than one mesh.
	constexpr uint c_MaxStaticMeshActorMeshes = 32;
	constexpr uint c_MaxStaticMeshActorEntities = c_MaxStaticMeshActorMeshes + 1;

	// One mesh's worth of data needed to build a static mesh actor entity. The bounds are
	// supplied directly rather than looked up here (they're already stored on the mesh
	// asset's own header at import time) - building an actor stays synchronous and doesn't
	// need to know anything about asset loading.
	struct StaticMeshActorMeshDesc
	{
		AssetID mesh;
		Transform localTransform;
		Vector3 aabbMin;
		Vector3 aabbMax;
		LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes> materialOverrides;
	};

	class TYR_ENGINE_API ActorUtil final
	{
	public:
		// A single mesh - one entity with a ComponentTransform, MeshComponent and
		// BoxComponent, no parent. Just BuildStaticMeshesActor with one mesh - see its comment.
		static Entity BuildStaticMeshActor(EntitySystem& entities, const StaticMeshActorMeshDesc& mesh);

		// One or more meshes, e.g. the separate walls/floor/ceiling of an imported building.
		// A single mesh gets one entity carrying the transform/mesh/box directly, no parent -
		// same shape as BuildStaticMeshActor. More than one mesh gets a shared root entity
		// (ComponentTransform only, identity, no parent) plus one child entity per mesh, each
		// parented to the root - moving the root moves every mesh with it. Returns every
		// entity created, root first when there is one.
		static LocalArray<Entity, c_MaxStaticMeshActorEntities> BuildStaticMeshesActor(EntitySystem& entities, const StaticMeshActorMeshDesc* meshes, uint count);
	};
}
