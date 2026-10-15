#include "ActorReflection.h"
#include "ECS/Components.h"

namespace tyr
{
	namespace
	{
		ComponentTransform CreateIdentityTransform()
		{
			ComponentTransform transform;
			transform.local.position = Vector3::c_Zero;
			transform.local.rotation = Quaternion::c_Identity;
			transform.local.scale = Vector3::c_One;
			transform.world = transform.local;
			return transform;
		}
	}

	// Starts with no mesh, which is set from the editor's properties. Collides with the mesh's triangles.
	TYR_ACTOR_START(StaticMeshActor)
		ColliderComponent collider{};
		collider.shape = static_cast<uint8>(ColliderShape::Mesh);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(collider);
	TYR_ACTOR_END()

	// A mesh moved by physics, such as something that can be knocked over or thrown.
	TYR_ACTOR_START(DynamicMeshActor)
		ColliderComponent collider{};
		collider.shape = static_cast<uint8>(ColliderShape::ConvexHull);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(collider);
		root.AddComponent(RigidBodyComponent{});
	TYR_ACTOR_END()

	// A skinned mesh with an animator. Both are set from the editor's properties.
	TYR_ACTOR_START(SkeletalMeshActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(SkeletalMeshComponent{});
		root.AddComponent(AnimatorComponent{});
		root.AddComponent(BoxComponent{});
	TYR_ACTOR_END()

	TYR_ACTOR_START(DirLightActor)
		DirLightComponent light{};
		light.direction = Vector3::Normalize(Vector3(-0.4f, -0.8f, 0.4f));
		light.colour = Vector3::c_One;
		light.intensity = 3.0f;
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()

	TYR_ACTOR_START(PointLightActor)
		PointLightComponent light{};
		light.colour = Vector3::c_One;
		light.intensity = 5.0f;
		light.range = 10.0f;
		light.attenuation = Vector3(1.0f, 0.5f, 0.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()

	// Where the player starts, standing on the actor's position and facing its Z axis. The box
	// makes it pickable in the editor and is about the player's size.
	TYR_ACTOR_START(PlayerStartActor)
		BoxComponent box{};
		box.center = Vector3(0.0f, 0.9f, 0.0f);
		box.halfExtents = Vector3(0.3f, 0.9f, 0.3f);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(box);
	TYR_ACTOR_END()

	// The character the game spawns for the player at a PlayerStartActor.
	TYR_ACTOR_START(PlayerCharacterActor)
		actorTypeDesc.placeable = false;
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(CharacterComponent{});
		root.AddComponent(InventoryComponent{});
		root.AddComponent(HealthComponent{});
	TYR_ACTOR_END()

	// A door that swings on its hinge, which is the actor's position. Its kinematic box, sized
	// from the mesh, pushes the player out of the way.
	TYR_ACTOR_START(DoorActor)
		RigidBodyComponent body{};
		body.motionType = static_cast<uint8>(MotionType::Kinematic);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(body);
		root.AddComponent(DoorComponent{});
	TYR_ACTOR_END()

	// A door that slides along its slide offset, such as an elevator or roller door.
	TYR_ACTOR_START(SlidingDoorActor)
		RigidBodyComponent body{};
		body.motionType = static_cast<uint8>(MotionType::Kinematic);
		DoorComponent door{};
		door.motion = static_cast<uint8>(DoorMotion::Slide);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(body);
		root.AddComponent(door);
	TYR_ACTOR_END()

	// A key panel or button on a wall that opens the door it names.
	TYR_ACTOR_START(DoorPanelActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(DoorPanelComponent{});
	TYR_ACTOR_END()

	// A ceiling light or lamp. Its light shines from the actor's position along its Z axis, over
	// the half of the world in front of it, so its mesh sits behind its origin. The mesh's emissive
	// materials glow while it's lit.
	TYR_ACTOR_START(LightFixtureActor)
		SpotLightComponent light{};
		light.colour = Vector3(1.0f, 0.95f, 0.88f);
		light.intensity = 8.0f;
		light.range = 8.0f;
		light.coneFalloff = 1.0f;
		light.attenuation = Vector3(1.0f, 0.0f, 1.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(light);
		root.AddComponent(LightFixtureComponent{});
	TYR_ACTOR_END()

	// A wall switch for the light fixtures on its circuit.
	TYR_ACTOR_START(LightSwitchActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(LightSwitchComponent{});
	TYR_ACTOR_END()

	// Gives the world power for its lights and door panels once the player starts it.
	TYR_ACTOR_START(GeneratorActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(GeneratorComponent{});
	TYR_ACTOR_END()

	// An item lying in the world, such as a key card, that the player can take.
	TYR_ACTOR_START(PickupActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
		root.AddComponent(ColliderComponent{});
		root.AddComponent(PickupComponent{});
	TYR_ACTOR_END()

	// Projects its material down its -Y axis, so it lies flat on a floor until turned. Scaling it
	// sizes the box. The box component makes it pickable in the editor.
	TYR_ACTOR_START(DecalActor)
		BoxComponent box{};
		box.center = Vector3::c_Zero;
		box.halfExtents = Vector3(0.5f, 0.5f, 0.5f);

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(box);
		root.AddComponent(DecalComponent{});
	TYR_ACTOR_END()

	// Shines along its Z axis, so it's turned with the rotate gizmo.
	// A box the navmesh is built in. Its bounds don't take clicks in the viewport, since they're
	// usually big enough to hold the camera, so it's picked from the hierarchy.
	TYR_ACTOR_START(NavmeshBoundsActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(NavmeshBoundsComponent{});
	TYR_ACTOR_END()

	// A torch, with a narrow beam and a dim spill around it. The game gives the player one.
	TYR_ACTOR_START(FlashlightActor)
		SpotLightComponent light{};
		light.colour = Vector3(1.0f, 0.93f, 0.82f);
		light.intensity = 250.0f;
		light.range = 30.0f;
		light.coneFalloff = 16.0f;
		light.attenuation = Vector3(1.0f, 0.0f, 1.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
		root.AddComponent(FlashlightComponent{});
	TYR_ACTOR_END()

	TYR_ACTOR_START(SpotLightActor)
		SpotLightComponent light{};
		light.colour = Vector3::c_One;
		light.intensity = 5.0f;
		light.range = 15.0f;
		light.coneFalloff = 8.0f;
		light.attenuation = Vector3(1.0f, 0.5f, 0.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()
}
