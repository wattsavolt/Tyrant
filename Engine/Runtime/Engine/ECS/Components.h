#pragma once

#include "AudioTypes.h"
#include "EngineMacros.h"
#include "Math/Quaternion.h"
#include "Math/Vector3.h"
#include "AssetSystem/AssetDataTypes.h"
#include "AssetSystem/SkeletonAsset.h"
#include "ECS/EntitySystem.h"
#include "RenderBase/RenderHandles.h"
#include "PhysicsTypes.h"
#include "Gameplay/GameplayTypes.h"
#include "NavmeshTypes.h"
#include "Actor/ActorTypes.h"

namespace tyr
{
	constexpr uint MaxChildComponents = 4u;

	struct Transform
	{
		Vector3 scale;
		Quaternion rotation;
		Vector3 position;
	};

	struct TransformComponent
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
		// Scales the glow of its materials' emissive, such as 0 for a lamp that's switched off.
		float emissiveScale = 1.0f;
		// How strongly it shines to show it can be picked up. Set while playing, not saved.
		float highlight = 0.0f;
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

	// One clip being played, how far through it is, and how far it has faded in over the clips
	// under it.
	struct AnimationPlayback
	{
		AssetID clip;
		float time = 0.0f;
		float playRate = 1.0f;
		// From 0 to 1 over fadeDuration seconds.
		float fade = 1.0f;
		float fadeDuration = 0.0f;
		bool loop = true;
		// Keeps the root bone from travelling across the ground, for clips played while
		// something else moves the character, such as walking on the navmesh.
		bool inPlace = false;
		// Where through the clip to start, as a share of its length, once it has loaded. Negative once started.
		float startPhase = -1.0f;
	};

	constexpr uint c_MaxBlendedClips = 4;

	// Plays clips on the skeletal mesh on the same entity. Each new clip fades in over the ones
	// already playing, and a second clip can be layered over one bone and every bone under it.
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

		// Worked out while playing. Not saved. The last one is the newest, fading in over the rest.
		LocalArray<AnimationPlayback, c_MaxBlendedClips> blends;
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
		// The tallest step it walks up.
		float stepHeight = 0.35f;
		// Set by gameplay. Not saved. moveVelocity is horizontal, in metres per second.
		Vector3 moveVelocity;
		bool wantsCrouch = false;
		// Set by physics. Not saved. Standing up waits until there's room overhead.
		bool grounded = false;
		bool crouched = false;
		// Its last two fixed step positions, blended between for rendering.
		Vector3 previousPosition;
		Vector3 currentPosition;
		uint characterIndex = PhysicsConstants::c_InvalidCharacter;
		// Set by gameplay. Not saved. A frozen character isn't moved at all, not even by gravity, and
		// one to teleport is put at the position on the next physics step.
		bool frozen = false;
		bool teleport = false;
		Vector3 teleportPosition;
	};

	// Walks the CharacterComponent on the same entity along the navmesh to a target, turning to
	// face the way it goes and opening unlocked doors in its way.
	struct NavAgentComponent
	{
		// Metres per second, and per second per second.
		float speed = 2.0f;
		float acceleration = 10.0f;
		// Degrees per second.
		float turnSpeed = 360.0f;
		// How close to the target counts as there. Other characters can't be got closer to than
		// both their radii.
		float arriveDistance = 1.0f;
		// A closed door in its way is hit this many times before it opens, a hit every so many seconds,
		// each landing a moment into the swing. Zero opens doors the way a person would.
		int doorBashes = 0;
		float doorBashInterval = 1.1f;
		float doorBashHitDelay = 0.5f;
		// It walks up until the door is this close, about as far as its swing reaches, before it starts.
		float doorBashReach = 0.75f;
		// Only those that may change floors walk stairs. Others stop at the top or bottom of a marked flight.
		bool usesStairs = false;

		// Set by gameplay, through NavAgents. Not saved.
		Vector3 target;
		bool hasTarget = false;
		// Set when it gets there, or gives up because it can't. Cleared by a new target. Not saved.
		bool arrived = false;
		bool stuck = false;

		// Worked out while walking. Not saved.
		NavPath path;
		// The corner it's walking to, and where the target was when the path was found.
		uint nextCorner = 0;
		Vector3 pathTarget;
		float repathTimer = 0.0f;
		Vector3 velocity;
		// Radians about Y from +Z, the way it faces.
		float yaw = 0.0f;
		bool yawKnown = false;
		// Set through NavAgents::FaceTowards. It turns to face it while standing still.
		Vector3 facePoint;
		bool hasFacePoint = false;
		// It's stuck when it barely moves for a while, a few times in a row.
		float stuckTimer = 0.0f;
		Vector3 stuckCheckPosition;
		uint stuckCount = 0;
		// Seconds it stays where it is before setting off, such as while getting up.
		float holdTimer = 0.0f;
		// Set through NavAgents::Dash: how fast it goes and for how many more seconds.
		Vector3 dashVelocity;
		float dashTimer = 0.0f;
		// The door it's bashing on, how many hits have landed, and each swing, counted up as it starts.
		Entity bashDoor = c_InvalidEntity;
		int bashCount = 0;
		float bashTimer = 0.0f;
		float bashHitTimer = -1.0f;
		uint bashSwing = 0;
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

	// A box in the entity's own local space, relative to its TransformComponent - for
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
		// Switched off while the camera is in an IndoorVolumeComponent, so it costs nothing there.
		bool outdoor = false;
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
		// Switched off while the camera is in an IndoorVolumeComponent, so it costs nothing there.
		bool outdoor = false;
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

	// A door that opens and closes when used, moving its own entity. Swinging doors turn about
	// their origin, so a door mesh's hinge goes there. Needs a kinematic body to push things aside.
	struct DoorComponent
	{
		// A DoorMotion: 0 swing, 1 slide.
		uint8 motion = static_cast<uint8>(DoorMotion::Swing);
		// Swing only: how far it turns, in degrees.
		float openAngle = 100.0f;
		// Swing only: it turns away from whoever opens it. Otherwise it always turns the same way.
		bool opensAway = true;
		// Slide only: how far it moves when fully open, in its own space.
		Vector3 slideOffset = Vector3(0.9f, 0.0f, 0.0f);
		// Seconds to fully open or close, and to fly open when something bursts through it.
		float openTime = 0.8f;
		float burstOpenTime = 0.25f;
		// A DoorLock: 0 unlocked, 1 needs key, 2 locked from the back, 3 locked.
		uint8 lock = static_cast<uint8>(DoorLock::Unlocked);
		// The item that unlocks it when it needs a key.
		ItemName key;
		// Another door that opens and closes with this one, such as the other half of a double door.
		ActorName linkedDoor;
		// Played at the door as it starts opening or closing, and when it won't open.
		SoundName openSound = "event:/Doors/Open";
		SoundName closeSound = "event:/Doors/Close";
		SoundName lockedSound = "event:/Doors/Locked";
		// Played at the door when something bashes on it.
		SoundName bashSound = "event:/Doors/Bash";
		// How far away enemies hear it open, close or get bashed, in metres. Rattling it locked carries half as far.
		float noiseRadius = 6.0f;

		// Worked out while playing. Not saved. Amounts go from 0 closed to 1 open.
		float openAmount = 0.0f;
		float targetAmount = 0.0f;
		// 1 or -1, the way it turns this time.
		float swingSign = 1.0f;
		// Where it was placed, which is where it's closed.
		bool closedPoseKnown = false;
		Vector3 closedPosition;
		Quaternion closedRotation;
		// Not saved. Broken off, it falls flat about its bottom edge, 1 or -1 being which way, and stays there.
		bool broken = false;
		float fallAmount = 0.0f;
		float fallSign = 1.0f;
		// Not saved. Flying open, burst through, until it's fully open.
		bool bursting = false;
	};

	// A wall panel or button that opens and closes a door. With a key, it unlocks the door for good
	// once the user holds the key.
	struct DoorPanelComponent
	{
		ActorName door;
		// Empty for a button that needs no key.
		ItemName key;
		// Has its own power. Otherwise it only works while the world has power, such as from a generator.
		bool powered = true;
		// Played at the panel whenever it's pressed.
		SoundName useSound = "event:/Doors/Button";
	};

	// A light fitting whose mesh glows and whose light shines while it's switched on and the world
	// has power. Light switches on the same circuit turn it on and off.
	struct LightFixtureComponent
	{
		CircuitName circuit;
		// A LightFixtureState: 0 working, 1 broken, 2 flickering.
		uint8 state = static_cast<uint8>(LightFixtureState::Working);
		bool switchedOn = true;

		// Worked out while running. Not saved. Brightness goes from 0 off to 1 fully on.
		float brightness = -1.0f;
		float flickerTimer = 0.0f;
		uint flickerSeed = 0;
		bool flickerLit = true;
	};

	// A wall switch that turns the light fixtures on its circuit on and off.
	struct LightSwitchComponent
	{
		CircuitName circuit;
		bool switchedOn = true;
		SoundName useSound = "event:/World/Switch";

		// Worked out while running. Not saved. Whether its mesh is turned to show it on, as placed.
		bool shownOn = true;
	};

	// A light the player can carry, with a spot light on the same entity. While held, the world
	// keeps it at the camera, offset from the eyes, turning after the view a little behind it.
	// Its light is dark while it's off.
	struct FlashlightComponent
	{
		bool on = true;
		// Right, up and forward from the eyes, in metres. It's held out in the left hand.
		Vector3 offset = Vector3(-0.16f, -0.135f, 0.53f);
		// How quickly it turns after the view. Higher keeps it closer.
		float followSpeed = 16.0f;
		// What its beam counts as lit for gameplay, such as holding enemies in it: out to this
		// range, and within this angle of the middle, in degrees.
		float beamRange = 12.0f;
		float beamAngle = 18.0f;
		// Played at it when it's switched on or off.
		SoundName toggleSound = "event:/Items/FlashlightClick";
		// Set by gameplay. Not saved.
		bool held = false;
		// How far it's drawn back towards the eyes, so it doesn't poke through a wall in front. Not saved.
		float pullBack = 0.0f;
	};

	// Leaves a trail of scent behind as the entity moves, which enemies can smell and follow.
	struct ScentComponent
	{
		// Metres between markers, and seconds each lasts.
		float spacing = 1.0f;
		float lifetime = 45.0f;
		// Where the last marker was left. Not saved.
		Vector3 lastMarker;
		bool hasMarker = false;
	};

	// Something enemies hunt, such as the player. Its noises, scent and sight all lead them to it.
	struct PerceivableComponent
	{
		// How easy it is to see, from 0 to 1. Lower is harder, such as when hiding.
		float visibility = 1.0f;
		// Not saved. Hidden away, such as in a wardrobe, nothing can sense it at all.
		bool hidden = false;
	};

	// What an enemy can sense, and what it knows from it. It hunts entities with a
	// PerceivableComponent.
	struct PerceptionComponent
	{
		// Anything it hunts within this distance it always knows of, such as one bumping into it.
		float touchRange = 0.9f;
		// Anything it hunts this close in front of it, with nothing in the way, it senses without
		// seeing. The angle is the whole width in front of it, in degrees.
		bool nearby = true;
		float nearbyRange = 3.0f;
		float nearbyAngle = 200.0f;
		// Once it's after its target, it keeps track of where it is for as long as nothing's in the
		// way between them, out to this distance. 0 for one that can't see.
		float trackRange = 25.0f;
		// Noises are heard out to the distance they carry times this, either straight through walls and
		// shut doors, each of which halves it, or round through doorways, where only shut doors do.
		// A heard noise is only walked to, unless it's its target's while it's already after it.
		bool hearing = true;
		float hearingScale = 1.0f;
		bool smell = false;
		// How far from its nose it smells scent, and the oldest scent it notices, in seconds.
		float smellRange = 3.0f;
		float smellMaxAge = 30.0f;
		bool sight = false;
		float sightRange = 15.0f;
		// The whole width of what it sees, in degrees.
		float sightAngle = 120.0f;

		// Worked out while playing. Not saved. What it knows of its target, and the noise it last
		// heard, with what made it if that's something it hunts.
		bool targetKnown = false;
		Entity target = c_InvalidEntity;
		Vector3 targetPosition;
		// Seconds since it last sensed where its target is, and how.
		float targetAge = 0.0f;
		uint8 targetSense = static_cast<uint8>(Sense::None);
		bool heardNoise = false;
		Vector3 noisePosition;
		Entity noiseSource = c_InvalidEntity;
		// How far the loudest noise heard this time carries, so quiet ones can be ignored, and the
		// newest noise it's listened for.
		float noiseRadius = 0.0f;
		uint lastNoiseId = 0;
		// Its target, heard while it was hidden, such as getting into or out of a hiding place.
		Entity heardHiddenTarget = c_InvalidEntity;
		// Set by its brain while it's after its target, so it's tracked.
		bool tracking = false;
		float updateTimer = 0.0f;
	};

	// How much harm something can take before it dies.
	struct HealthComponent
	{
		float maxHealth = 100.0f;
		// Played at it when it's hurt, and instead when it dies.
		SoundName hurtSound;
		SoundName deathSound;
		// Not saved. Full until the first damage.
		float health = 100.0f;
		bool damaged = false;
		// Not saved. Damage adds to it, up to 1, and whatever shows being hurt fades it back down.
		float hurt = 0.0f;
		// Not saved. Counts the times it's been hurt, so others can notice a new hit.
		uint damageCount = 0;
	};

	// Keeps a skeletal mesh's limbs out of walls and doors while playing, with a capsule along each of
	// its longer bones. An arm that sinks into something is swung back out at the shoulder; anything
	// else pushes its CharacterComponent away across the ground.
	struct LimbCollisionComponent
	{
		float limbRadius = 0.05f;
		// Bones shorter than this, such as fingers and the face's, are left out.
		float minBoneLength = 0.06f;
		// Metres per second it's pushed at most.
		float maxPushSpeed = 2.5f;
		// The bone at each shoulder, how quickly an arm swings out of the way, and how far at most, in degrees.
		BoneName leftArm = "CC_Base_L_Upperarm";
		BoneName rightArm = "CC_Base_R_Upperarm";
		float armTurnSpeed = 14.0f;
		float maxArmTurn = 70.0f;
		// Worked out while playing. Not saved. Each arm's swing out of the way, relative to the mesh,
		// and its shoulder bone, once found.
		Quaternion armTurns[2] = { Quaternion::c_Identity, Quaternion::c_Identity };
		int armBones[2] = { -1, -1 };
		// Set by the game while its limbs shouldn't push it about, such as crouched feeding over a body.
		bool still = false;
	};

	// Indoors: a box around the entity, stretched by its scale. While the camera is in one, lights
	// marked outdoor are switched off.
	struct IndoorVolumeComponent
	{
		Vector3 halfExtents = Vector3(10.0f, 5.0f, 10.0f);
	};

	// A box around the entity, stretched by its scale, set off when the player walks into it. Whatever
	// waits on it, such as an enemy held as it starts, finds it by its actor's name.
	struct TriggerVolumeComponent
	{
		Vector3 halfExtents = Vector3(1.0f, 1.0f, 1.0f);
		// It stays set off once it has been, rather than only while the player's inside.
		bool once = true;

		// Worked out while playing. Saved.
		bool triggered = false;
	};

	// Where the navmesh is built: a box around the entity, stretched by its scale. Only floors
	// inside one are walkable for enemies. A level without any has its navmesh built everywhere.
	struct NavmeshBoundsComponent
	{
		Vector3 halfExtents = Vector3(10.0f, 5.0f, 10.0f);
	};

	// Marks the navmesh in a box around the entity, stretched by its scale, as one of the
	// NavmeshAreas, such as a flight of stairs only some enemies may take.
	struct NavmeshAreaComponent
	{
		Vector3 halfExtents = Vector3(1.0f, 1.0f, 1.0f);
		// 2 is stairs.
		uint8 area = 2;
	};

	// Something the player can pick up, such as a key card. It's removed from the world once taken.
	struct PickupComponent
	{
		ItemName item;
		// How many there are, such as a box of a few, and how many of the item stack in one inventory
		// slot. Only what fits is taken.
		uint8 count = 1;
		uint8 stackLimit = 1;
		// A note or a code, kept with the character's documents rather than taking an inventory slot.
		bool document = false;
		SoundName pickupSound = "event:/Items/Pickup";

		// How far through its shine's pulse it is. Not saved.
		float shineTime = 0.0f;
	};

	// A sound that plays for as long as the world is being played, such as ambience or a humming
	// machine. Whether it's heard from where it is, or everywhere, is up to how its event was authored.
	struct AmbientSoundComponent
	{
		SoundName sound;

		// Worked out while playing. Not saved.
		AudioEventHandle handle;
		bool started = false;
	};

	// The items a character carries, one slot each, though some stack several to a slot.
	struct InventoryComponent
	{
		LocalArray<ItemName, GameplayConstants::c_MaxInventoryItems> items;
		// How many slots it has, up to c_MaxInventoryItems.
		uint8 slots = GameplayConstants::c_MaxInventoryItems;
		// Keys are thrown away once there's nothing left they unlock.
		bool discardSpentKeys = false;

		// Not saved in levels. How many are stacked in each slot, and how many fit, as Inventory
		// keeps them.
		LocalArray<uint8, GameplayConstants::c_MaxInventoryItems> counts;
		LocalArray<uint8, GameplayConstants::c_MaxInventoryItems> stackLimits;
	};

	// The notes and codes a character has picked up. They don't take inventory slots, but still
	// open whatever they're the key to.
	struct DocumentsComponent
	{
		LocalArray<ItemName, GameplayConstants::c_MaxDocuments> documents;
	};

	// Something the game itself handles when it's used, such as a locker or a typewriter, named by
	// its kind. The game looks for ones that have been used and clears them.
	struct ActivatorComponent
	{
		ItemName kind;
		// Shown after the use button, such as "Open locker".
		ItemName action = "Use";

		// Not saved. Set when it's used, until the game deals with it.
		bool used = false;
	};

}
