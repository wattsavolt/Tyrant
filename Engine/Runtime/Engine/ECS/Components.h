#pragma once

#include "EngineMacros.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "AssetSystem/AssetDataTypes.h"
#include "ECS/EntitySystem.h"
#include "RenderBase/RenderHandles.h"

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

	struct MeshComponent
	{
		AssetID mesh;
		// One material per submesh. An invalid or missing entry uses the mesh's own material,
		// and is filled in with it once the mesh instance has been created.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materials;
		// Invalid until the mesh and its materials have loaded.
		MeshInstanceHandle meshInstance;
		// True once the mesh instance has been asked for, even if it hasn't been created yet.
		bool meshInstanceRequested = false;
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
		// Invalid until the light has been created in the renderer.
		DirLightHandle lightHandle;
	};

	// A light shining in every direction from the entity's position.
	struct PointLightComponent
	{
		Vector3 colour;
		float intensity;
		// Nothing further away than this is lit.
		float range;
		// How the light fades with distance: constant, linear and squared.
		Vector3 attenuation;
		bool castsShadow;
		// Invalid until the light has been created in the renderer.
		PointLightHandle lightHandle;
	};

	// A light shining in a cone from the entity's position, along its Z axis.
	struct SpotLightComponent
	{
		Vector3 colour;
		float intensity;
		// Nothing further away than this is lit.
		float range;
		// How quickly the light fades away from the centre of the cone. Higher is narrower.
		float coneFalloff;
		// How the light fades with distance: constant, linear and squared.
		Vector3 attenuation;
		bool castsShadow;
		// Invalid until the light has been created in the renderer.
		SpotLightHandle lightHandle;
	};

}