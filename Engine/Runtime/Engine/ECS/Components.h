#pragma once

#include "EngineMacros.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "AssetSystem/AssetDataTypes.h"
#include "ECS/EntitySystem.h"
#include "RenderBase/RenderHandles.h"
#include "Physics/PhysicsTypes.h"

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
		// One material per submesh. An invalid or missing entry uses the mesh's own material.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materials;
		// The materials the mesh instance actually uses, once it's been created. Not saved.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> loadedMaterials;
		// Invalid until the mesh and its materials have loaded.
		MeshInstanceHandle meshInstance;
		// True once the mesh instance has been asked for, even if it hasn't been created yet.
		bool meshInstanceRequested = false;
	};

	// A skinned mesh, posed by the AnimatorComponent on the same entity if it has one, otherwise
	// left in its bind pose.
	struct SkeletalMeshComponent
	{
		AssetID mesh;
		// One material per submesh. An invalid or missing entry uses the mesh's own material.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> materials;
		// The materials the instance actually uses, once it's been created. Not saved.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes> loadedMaterials;
		// Invalid until the mesh, its skeleton and its materials have loaded.
		SkeletalMeshInstanceHandle meshInstance;
		// True once the instance has been asked for, even if it hasn't been created yet.
		bool meshInstanceRequested = false;
	};

	// One clip being played and how far through it is.
	struct AnimationPlayback
	{
		AssetID clip;
		float time = 0.0f;
	};

	// Plays clips on the skeletal mesh on the same entity. The base clip can cross-fade to
	// another, and a second clip can be layered over one bone and every bone under it.
	struct AnimatorComponent
	{
		// Changing it cross-fades to the new clip over fadeTime seconds while simulating.
		AssetID clip;
		float fadeTime = 0.25f;
		float playRate = 1.0f;
		bool loop = true;
		// Played over layerMaskBone and the bones under it, such as the upper body.
		AssetID layerClip;
		uint layerMaskBone = 0;
		float layerWeight = 1.0f;

		// Worked out while playing. Not saved.
		AnimationPlayback current;
		// The clip being faded out of, while fadeElapsed is less than fadeDuration.
		AnimationPlayback previous;
		float fadeDuration = 0.0f;
		float fadeElapsed = 0.0f;
		AnimationPlayback layer;
	};

	// A collision shape in the entity's local space, scaled with the entity. Without a
	// RigidBodyComponent on the same entity it never moves. Only simulated worlds have bodies.
	// Mesh and convex hull shapes come from the collision mesh made when the entity's mesh was imported.
	struct ColliderComponent
	{
		// A ColliderShape: 0 box, 1 sphere, 2 capsule, 3 mesh, 4 convex hull.
		uint8 shape = static_cast<uint8>(ColliderShape::Box);
		Vector3 center;
		// Box only. Left at zero, it takes the size of the entity's mesh once that loads.
		Vector3 halfExtents;
		// Sphere and capsule.
		float radius = 0.5f;
		// Capsule only: half the length of its straight middle.
		float halfHeight = 0.5f;
		float friction = 0.5f;
		// How bouncy it is, from 0 to 1.
		float restitution = 0.0f;
		// The physics body made from it. Not saved.
		uint bodyID = PhysicsConstants::c_InvalidBodyID;
		// The mesh's collision mesh, held loaded while the shape is mesh or convex hull. Not saved.
		AssetID collisionMesh;
	};

	// An upright capsule moved by gameplay, such as the player, rather than by the simulation. It
	// slides along walls, walks up steps and falls. The entity's position is the capsule's base.
	struct CharacterComponent
	{
		float radius = 0.3f;
		float height = 1.75f;
		float crouchHeight = 1.15f;
		// The steepest slope it can walk up, in degrees.
		float maxSlope = 50.0f;
		// The tallest step it walks up without jumping.
		float stepHeight = 0.35f;
		// Set by gameplay. Not saved. moveVelocity is horizontal, in metres per second, and a
		// jump speed is used by the next physics step on the ground, then cleared.
		Vector3 moveVelocity;
		float jumpSpeed = 0.0f;
		bool wantsCrouch = false;
		// Set by physics. Not saved. Standing up waits until there's room overhead.
		bool grounded = false;
		bool crouched = false;
		// Its last two fixed step positions, blended between for rendering.
		Vector3 previousPosition;
		Vector3 currentPosition;
		uint characterIndex = PhysicsConstants::c_InvalidCharacter;
	};

	// Makes the ColliderComponent on the same entity move.
	struct RigidBodyComponent
	{
		// A MotionType: 0 static, 1 kinematic, 2 dynamic.
		uint8 motionType = static_cast<uint8>(MotionType::Dynamic);
		// In kilograms. Zero works it out from the collider's size.
		float mass = 0.0f;
		float linearDamping = 0.05f;
		float angularDamping = 0.05f;
		float gravityFactor = 1.0f;
		// The body's last two fixed step transforms, blended between for rendering. Not saved.
		Vector3 previousPosition;
		Quaternion previousRotation;
		Vector3 currentPosition;
		Quaternion currentRotation;
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

	// Projects a material down the entity's -Y axis onto whatever is inside its box, which is a
	// metre on each side before the entity's scale. The material's albedo alpha is its coverage.
	struct DecalComponent
	{
		AssetID material;
		float opacity = 1.0f;
		// How much of the box's depth, from each end, it fades out over, from 0 to 1.
		float fadeDepth = 0.5f;
		bool affectsColour = true;
		bool affectsNormal = true;
		// Roughness and metallic.
		bool affectsRoughness = true;
		// Higher draws over lower where decals overlap.
		int sortOrder = 0;
		// The material it holds loaded, and the renderer's decal made once it has. Not saved.
		AssetID loadedMaterial;
		DecalHandle decalHandle;
	};

}