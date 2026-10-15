#include "PhysicsComponentsManager.h"
#include "PhysicsModule.h"
#include "PhysicsManager.h"
#include "PhysicsScene.h"
#include "PhysicsShapes.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetUtil.h"
#include "Math/Math.h"
#include <algorithm>
#include <cmath>

namespace tyr
{
	namespace
	{
		// Null until the collision mesh has loaded, or when it has no shape this body can use.
		// Comes with a reference for the caller to release.
		const PhysicsShape* CreateMeshShape(const ColliderComponent& collider, const Vector3& scale, MotionType motionType, const AssetManager& assetManager)
		{
			const CollisionMesh* collisionMesh = assetManager.GetCollisionMesh(collider.collisionMesh);
			if (!collisionMesh)
			{
				return nullptr;
			}
			const bool triangles = static_cast<ColliderShape>(collider.shape) == ColliderShape::Mesh && motionType != MotionType::Dynamic;
			// The shape is shared by every entity with this mesh, so each body scales its own copy.
			return PhysicsShapes::CreateScaled(triangles ? collisionMesh->triangleShape : collisionMesh->convexShape, scale);
		}

		// Sizes are scaled by the entity. Spheres and capsules stay round, using the largest scale.
		// Comes with a reference for the caller to release.
		const PhysicsShape* CreateShape(const ColliderComponent& collider, const Vector3& scale)
		{
			const Vector3 absScale(std::abs(scale.x), std::abs(scale.y), std::abs(scale.z));
			const Vector3 offset = collider.center * scale;
			switch (static_cast<ColliderShape>(collider.shape))
			{
			case ColliderShape::Sphere:
				return PhysicsShapes::CreateSphere(collider.radius * std::max(absScale.x, std::max(absScale.y, absScale.z)), offset);
			case ColliderShape::Capsule:
				return PhysicsShapes::CreateCapsule(collider.halfHeight * absScale.y, collider.radius * std::max(absScale.x, absScale.z), offset);
			default:
				return PhysicsShapes::CreateBox(collider.halfExtents * absScale, offset);
			}
		}

		bool HasSize(const ColliderComponent& collider)
		{
			switch (static_cast<ColliderShape>(collider.shape))
			{
			case ColliderShape::Sphere:
				return collider.radius > 0.0f;
			case ColliderShape::Capsule:
				return collider.radius > 0.0f && collider.halfHeight > 0.0f;
			case ColliderShape::Mesh:
			case ColliderShape::ConvexHull:
				return AssetUtil::IsValidAssetID(collider.collisionMesh);
			default:
				return collider.halfExtents.x > 0.0f && collider.halfExtents.y > 0.0f && collider.halfExtents.z > 0.0f;
			}
		}
	}

	const PhysicsShape* PhysicsComponentsManager::CreateColliderShape(const ColliderComponent& collider, const Vector3& scale, MotionType motionType, const AssetManager& assetManager)
	{
		if (!HasSize(collider))
		{
			return nullptr;
		}
		const ColliderShape colliderShape = static_cast<ColliderShape>(collider.shape);
		const bool fromMesh = colliderShape == ColliderShape::Mesh || colliderShape == ColliderShape::ConvexHull;
		return fromMesh ? CreateMeshShape(collider, scale, motionType, assetManager) : CreateShape(collider, scale);
	}

	PhysicsComponentsManager::PhysicsComponentsManager()
	{
		PhysicsModule* physicsModule;
		TYR_GET_MODULE(PhysicsModule, physicsModule);
		m_PhysicsManager = physicsModule->GetPhysicsManager();
	}

	Handle PhysicsComponentsManager::CreatePhysicsWorld()
	{
		return m_PhysicsManager->CreatePhysicsWorld();
	}

	void PhysicsComponentsManager::DestroyPhysicsWorld(Handle physicsWorld)
	{
		m_PhysicsManager->DestroyPhysicsWorld(physicsWorld);
	}

	void PhysicsComponentsManager::Update(Handle physicsWorld, World& world, const AssetManager& assetManager, float deltaTime)
	{
		PhysicsScene& scene = m_PhysicsManager->GetScene(physicsWorld);
		CreateBodies(scene, world, assetManager);
		CreateCharacters(scene, world);

		const uint steps = m_PhysicsManager->AdvanceTime(physicsWorld, deltaTime);
		for (uint i = 0; i < steps; ++i)
		{
			Step(scene, world);
		}
	}

	float PhysicsComponentsManager::GetInterpolation(Handle physicsWorld) const
	{
		return m_PhysicsManager->GetInterpolation(physicsWorld);
	}

	void PhysicsComponentsManager::RemoveBody(Handle physicsWorld, ColliderComponent& collider)
	{
		if (collider.bodyID == PhysicsConstants::c_InvalidBodyID)
		{
			return;
		}
		m_PhysicsManager->GetScene(physicsWorld).RemoveBody(collider.bodyID);
		collider.bodyID = PhysicsConstants::c_InvalidBodyID;
	}

	void PhysicsComponentsManager::RemoveCharacter(Handle physicsWorld, CharacterComponent& character)
	{
		if (character.characterIndex == PhysicsConstants::c_InvalidCharacter)
		{
			return;
		}
		m_PhysicsManager->GetScene(physicsWorld).RemoveCharacter(character.characterIndex);
		character.characterIndex = PhysicsConstants::c_InvalidCharacter;
	}

	void PhysicsComponentsManager::CreateCharacters(PhysicsScene& scene, World& world)
	{
		world.entities.ForEach<CharacterComponent>([&](Entity entity, CharacterComponent& character)
		{
			if (character.characterIndex != PhysicsConstants::c_InvalidCharacter || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}

			CharacterDesc desc;
			desc.position = world.entities.GetComponent<ComponentTransform>(entity).world.position;
			desc.radius = character.radius;
			desc.height = character.height;
			desc.crouchHeight = character.crouchHeight;
			desc.maxSlope = character.maxSlope;
			desc.userData = static_cast<uint64>(entity);
			const uint index = scene.AddCharacter(desc);
			if (index == PhysicsConstants::c_InvalidCharacter)
			{
				TYR_LOG_ERROR("A world can't have more than %u characters.", PhysicsConstants::c_MaxCharacters);
				return;
			}
			character.characterIndex = index;
			character.previousPosition = desc.position;
			character.currentPosition = desc.position;
			character.crouched = false;
		});
	}

	void PhysicsComponentsManager::StepCharacters(PhysicsScene& scene, World& world)
	{
		world.entities.ForEach<CharacterComponent>([&](Entity /*entity*/, CharacterComponent& character)
		{
			if (character.characterIndex == PhysicsConstants::c_InvalidCharacter)
			{
				return;
			}
			CharacterMove move;
			move.moveVelocity = character.moveVelocity;
			move.wantsCrouch = character.wantsCrouch;
			move.stepHeight = character.stepHeight;
			const CharacterState state = scene.MoveCharacter(character.characterIndex, move, PhysicsConstants::c_FixedTimeStep);

			character.previousPosition = character.currentPosition;
			character.currentPosition = state.position;
			character.grounded = state.grounded;
			character.crouched = state.crouched;
		});
	}

	bool PhysicsComponentsManager::Raycast(Handle physicsWorld, const Vector3& origin, const Vector3& direction, float maxDistance, RayCastEntityHit& outHit) const
	{
		PhysicsRayHit hit;
		if (!m_PhysicsManager->GetScene(physicsWorld).Raycast(origin, direction, maxDistance, hit))
		{
			return false;
		}
		outHit.entity = static_cast<Entity>(hit.userData);
		outHit.position = hit.position;
		outHit.normal = hit.normal;
		outHit.distance = hit.distance;
		return true;
	}

	void PhysicsComponentsManager::CreateBodies(PhysicsScene& scene, World& world, const AssetManager& assetManager)
	{
		bool addedAny = false;
		world.entities.ForEach<ColliderComponent>([&](Entity entity, ColliderComponent& collider)
		{
			// A box sized from its mesh waits for the mesh to load.
			if (collider.bodyID != PhysicsConstants::c_InvalidBodyID || !HasSize(collider) || !world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}

			MotionType motionType = MotionType::Static;
			RigidBodyComponent* rigidBody = nullptr;
			if (world.entities.HasComponent<RigidBodyComponent>(entity))
			{
				rigidBody = &world.entities.GetComponent<RigidBodyComponent>(entity);
				motionType = static_cast<MotionType>(rigidBody->motionType);
			}

			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
			const ColliderShape colliderShape = static_cast<ColliderShape>(collider.shape);
			const PhysicsShape* shape = CreateColliderShape(collider, transform.scale, motionType, assetManager);
			if (!shape)
			{
				return;
			}

			BodyDesc desc;
			desc.shape = shape;
			desc.position = transform.position;
			desc.rotation = transform.rotation;
			desc.motion = motionType;
			desc.friction = collider.friction;
			desc.restitution = collider.restitution;
			desc.userData = static_cast<uint64>(entity);
			if (rigidBody)
			{
				desc.linearDamping = rigidBody->linearDamping;
				desc.angularDamping = rigidBody->angularDamping;
				desc.gravityFactor = rigidBody->gravityFactor;
				desc.mass = rigidBody->mass;
				// A triangle mesh has no volume to work its mass out from. Only a kinematic one gets here.
				desc.massFromBounds = motionType == MotionType::Kinematic && colliderShape == ColliderShape::Mesh;

				rigidBody->previousPosition = transform.position;
				rigidBody->previousRotation = transform.rotation;
				rigidBody->currentPosition = transform.position;
				rigidBody->currentRotation = transform.rotation;
			}

			const uint body = scene.AddBody(desc);
			// The body holds its own reference.
			PhysicsShapes::Release(shape);
			if (body == PhysicsConstants::c_InvalidBodyID)
			{
				TYR_LOG_ERROR("Out of physics bodies.");
				return;
			}
			collider.bodyID = body;
			addedAny = true;
		});

		if (addedAny)
		{
			scene.OptimizeBroadPhase();
		}
	}

	void PhysicsComponentsManager::Step(PhysicsScene& scene, World& world)
	{
		// Kinematic bodies are moved to wherever gameplay put their entity, over the step.
		world.entities.ForEach<RigidBodyComponent>([&](Entity entity, RigidBodyComponent& rigidBody)
		{
			if (static_cast<MotionType>(rigidBody.motionType) != MotionType::Kinematic || !world.entities.HasComponent<ColliderComponent>(entity))
			{
				return;
			}
			const ColliderComponent& collider = world.entities.GetComponent<ColliderComponent>(entity);
			if (collider.bodyID != PhysicsConstants::c_InvalidBodyID)
			{
				const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
				scene.MoveKinematic(collider.bodyID, transform.position, transform.rotation, PhysicsConstants::c_FixedTimeStep);
			}
		});

		scene.Step(PhysicsConstants::c_FixedTimeStep);
		StepCharacters(scene, world);

		world.entities.ForEach<RigidBodyComponent>([&](Entity entity, RigidBodyComponent& rigidBody)
		{
			if (static_cast<MotionType>(rigidBody.motionType) != MotionType::Dynamic || !world.entities.HasComponent<ColliderComponent>(entity))
			{
				return;
			}
			const ColliderComponent& collider = world.entities.GetComponent<ColliderComponent>(entity);
			if (collider.bodyID == PhysicsConstants::c_InvalidBodyID)
			{
				return;
			}
			rigidBody.previousPosition = rigidBody.currentPosition;
			rigidBody.previousRotation = rigidBody.currentRotation;
			scene.GetBodyTransform(collider.bodyID, rigidBody.currentPosition, rigidBody.currentRotation);
		});
	}
}
