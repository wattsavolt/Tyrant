#pragma once

#include "EngineMacros.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "AssetSystem/AssetDataTypes.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	constexpr uint MaxChildComponents = 4u;

	struct Transform
	{
		Vector3 scale;
		Quaternion rotation;
		Vector3 position;
	};

	struct ComponentTransform
	{
		Transform local;
		Transform world;
		Entity parentEntity = c_InvalidEntity;
	};

	// materialOverrides is a LocalArray, not Array - an ECS component can never own a
	// dynamic allocation (Archetype/EcsColumn moves components between archetypes with
	// memcpy, never placement-construct/destruct - see ComponentReflection.h's comment).
	// c_MaxSubmeshes is a safe upper bound (a mesh can't have more submeshes than
	// that to begin with), even though realistically an entity would only override one or two.
	struct MeshComponent
	{
		AssetID mesh;
		LocalArray<MaterialOverride, MeshConstants::c_MaxSubmeshes> materialOverrides;
	};

	// A box in the entity's own local space, relative to its ComponentTransform - for
	// physics (Jolt) to use later. Just data for now, nothing reads it yet.
	struct BoxComponent
	{
		Vector3 center;
		Vector3 halfExtents;
	};

	// One directional light instanced into the world per entity that has one - see
	// WorldManager::UpdateWorld/ShutdownWorld, which create/tear down the renderer-side
	// light from this the same way MeshComponent's mesh instance is.
	// Named DirLightComponent, not DirectionalLightComponent - the latter is 25 characters,
	// over TypeName's 23-char capacity (c_MaxTypeName, TypeName.h) that reflection stores
	// every type's name in.
	struct DirLightComponent
	{
		Vector3 direction;
		Vector3 colour;
		float intensity;
		bool castsShadow;
	};

}