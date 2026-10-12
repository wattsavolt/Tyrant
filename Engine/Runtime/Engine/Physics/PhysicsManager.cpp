#include "PhysicsManager.h"
#include "PhysicsJobSystem.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetUtil.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

namespace tyr
{
	namespace
	{
		constexpr JPH::uint c_MaxJobs = 2048;
		constexpr JPH::uint c_MaxBarriers = 8;
		constexpr size_t c_TempAllocatorSize = 32 * 1024 * 1024;
		// Jolt's recommended mutex count for most body counts.
		constexpr JPH::uint c_BodyMutexCount = 0;

		namespace ObjectLayers
		{
			constexpr JPH::ObjectLayer c_NonMoving = 0;
			constexpr JPH::ObjectLayer c_Moving = 1;
			constexpr JPH::uint c_Count = 2;
		}

		namespace BroadPhaseLayers
		{
			constexpr JPH::BroadPhaseLayer c_NonMoving(0);
			constexpr JPH::BroadPhaseLayer c_Moving(1);
			constexpr JPH::uint c_Count = 2;
		}

		// Things that never move are kept in their own broad phase tree and never collide with each other.
		class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface
		{
		public:
			JPH::uint GetNumBroadPhaseLayers() const override
			{
				return BroadPhaseLayers::c_Count;
			}

			JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
			{
				return layer == ObjectLayers::c_NonMoving ? BroadPhaseLayers::c_NonMoving : BroadPhaseLayers::c_Moving;
			}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
			{
				return layer == BroadPhaseLayers::c_NonMoving ? "NonMoving" : "Moving";
			}
#endif
		};

		class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
			{
				return layer == ObjectLayers::c_Moving || broadPhaseLayer == BroadPhaseLayers::c_Moving;
			}
		};

		class ObjectPairFilter final : public JPH::ObjectLayerPairFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
			{
				return a == ObjectLayers::c_Moving || b == ObjectLayers::c_Moving;
			}
		};

		// Jolt allocates through the engine's heap.
		void* Allocate(size_t size)
		{
			return MemAlloc(size);
		}

		void* Reallocate(void* block, size_t oldSize, size_t newSize)
		{
			void* newBlock = MemAlloc(newSize);
			if (block)
			{
				memcpy(newBlock, block, oldSize < newSize ? oldSize : newSize);
				Free(block);
			}
			return newBlock;
		}

		void FreeBlock(void* block)
		{
			Free(block);
		}

		void* AlignedAllocate(size_t size, size_t alignment)
		{
			return MemAllocAligned(size, alignment);
		}

		void AlignedFreeBlock(void* block)
		{
			FreeAligned(block);
		}

		void Trace(const char* format, ...)
		{
			char message[1024];
			va_list args;
			va_start(args, format);
			vsnprintf(message, sizeof(message), format, args);
			va_end(args);
			TYR_LOG_WARNING("Jolt: %s", message);
		}

#ifdef JPH_ENABLE_ASSERTS
		bool AssertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
		{
			TYR_LOG_ERROR("Jolt assert %s:%u: (%s) %s", file, line, expression, message ? message : "");
			// Breaks into the debugger.
			return true;
		}
#endif

		JPH::Vec3 ToJolt(const Vector3& v)
		{
			return JPH::Vec3(v.x, v.y, v.z);
		}

		JPH::Quat ToJolt(const Quaternion& q)
		{
			return JPH::Quat(q.x, q.y, q.z, q.w);
		}

		Vector3 FromJolt(JPH::Vec3Arg v)
		{
			return Vector3(v.GetX(), v.GetY(), v.GetZ());
		}

		Quaternion FromJolt(JPH::QuatArg q)
		{
			return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
		}

		// Null until the collision mesh has loaded, or when it has no shape this body can use.
		JPH::ShapeRefC CreateMeshShape(const ColliderComponent& collider, const Vector3& scale, MotionType motionType, const AssetManager& assetManager)
		{
			const CollisionMesh* collisionMesh = assetManager.GetCollisionMesh(collider.collisionMesh);
			if (!collisionMesh)
			{
				return nullptr;
			}
			const bool triangles = static_cast<ColliderShape>(collider.shape) == ColliderShape::Mesh && motionType != MotionType::Dynamic;
			const JPH::Shape* shape = triangles ? collisionMesh->triangleShape : collisionMesh->convexShape;
			if (!shape || scale == Vector3::c_One)
			{
				return shape;
			}

			// The shape is shared by every entity with this mesh, so each body scales its own copy.
			JPH::ShapeSettings::ShapeResult result = JPH::ScaledShapeSettings(shape, shape->MakeScaleValid(ToJolt(scale))).Create();
			if (result.HasError())
			{
				TYR_LOG_ERROR("Couldn't scale a mesh collider: %s", result.GetError().c_str());
				return nullptr;
			}
			return result.Get();
		}

		// Sizes are scaled by the entity. Spheres and capsules stay round, using the largest scale.
		JPH::ShapeRefC CreateShape(const ColliderComponent& collider, const Vector3& scale)
		{
			const float roundScale = std::max(std::abs(scale.x), std::max(std::abs(scale.y), std::abs(scale.z)));
			JPH::ShapeSettings::ShapeResult result;
			switch (static_cast<ColliderShape>(collider.shape))
			{
			case ColliderShape::Sphere:
				result = JPH::SphereShapeSettings(collider.radius * roundScale).Create();
				break;
			case ColliderShape::Capsule:
				result = JPH::CapsuleShapeSettings(collider.halfHeight * std::abs(scale.y), collider.radius * std::max(std::abs(scale.x), std::abs(scale.z))).Create();
				break;
			default:
			{
				const JPH::Vec3 halfExtents = ToJolt(collider.halfExtents * Vector3(std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)));
				// The rounded edge can't be thicker than the box.
				const float convexRadius = std::min(JPH::cDefaultConvexRadius, halfExtents.ReduceMin() * 0.5f);
				result = JPH::BoxShapeSettings(halfExtents, convexRadius).Create();
				break;
			}
			}

			if (result.HasError())
			{
				TYR_LOG_ERROR("Couldn't create a collider: %s", result.GetError().c_str());
				return nullptr;
			}

			if (collider.center == Vector3::c_Zero)
			{
				return result.Get();
			}

			JPH::ShapeSettings::ShapeResult offsetResult = JPH::RotatedTranslatedShapeSettings(ToJolt(collider.center * scale), JPH::Quat::sIdentity(), result.Get()).Create();
			return offsetResult.HasError() ? nullptr : offsetResult.Get();
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

	struct PhysicsWorldData
	{
		BroadPhaseLayerMap broadPhaseLayers;
		ObjectVsBroadPhaseFilter objectVsBroadPhaseFilter;
		ObjectPairFilter objectPairFilter;
		JPH::PhysicsSystem system;
	};

	PhysicsManager::PhysicsManager()
	{
		JPH::Allocate = &Allocate;
		JPH::Reallocate = &Reallocate;
		JPH::Free = &FreeBlock;
		JPH::AlignedAllocate = &AlignedAllocate;
		JPH::AlignedFree = &AlignedFreeBlock;
		JPH::Trace = &Trace;
		JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = &AssertFailed;)

		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		m_TempAllocator = new JPH::TempAllocatorImpl(c_TempAllocatorSize);
		m_JobSystem = new PhysicsJobSystem(c_MaxJobs, c_MaxBarriers);
	}

	PhysicsManager::~PhysicsManager()
	{
		delete m_JobSystem;
		delete m_TempAllocator;

		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	Handle PhysicsManager::CreatePhysicsWorld()
	{
		const Handle handle = m_Worlds.Create();
		PhysicsWorldEntry& entry = m_Worlds[handle];
		// Made when a game world starts and kept until it ends.
		entry.data = new PhysicsWorldData();
		entry.accumulator = 0.0f;

		PhysicsWorldData& data = *entry.data;
		data.system.Init(PhysicsConstants::c_MaxBodies, c_BodyMutexCount, PhysicsConstants::c_MaxBodyPairs, PhysicsConstants::c_MaxContactConstraints,
			data.broadPhaseLayers, data.objectVsBroadPhaseFilter, data.objectPairFilter);
		data.system.SetGravity(JPH::Vec3(0.0f, PhysicsConstants::c_Gravity, 0.0f));
		return handle;
	}

	void PhysicsManager::DestroyPhysicsWorld(Handle physicsWorld)
	{
		PhysicsWorldEntry& entry = m_Worlds[physicsWorld];
		JPH::BodyInterface& bodies = entry.data->system.GetBodyInterfaceNoLock();

		JPH::BodyIDVector bodyIDs;
		entry.data->system.GetBodies(bodyIDs);
		if (!bodyIDs.empty())
		{
			bodies.RemoveBodies(bodyIDs.data(), static_cast<int>(bodyIDs.size()));
			bodies.DestroyBodies(bodyIDs.data(), static_cast<int>(bodyIDs.size()));
		}

		delete entry.data;
		m_Worlds.Delete(physicsWorld);
	}

	void PhysicsManager::Update(Handle physicsWorld, World& world, const AssetManager& assetManager, float deltaTime)
	{
		PhysicsWorldEntry& entry = m_Worlds[physicsWorld];
		PhysicsWorldData& data = *entry.data;

		CreateBodies(data, world, assetManager);

		entry.accumulator += deltaTime;
		uint steps = 0;
		while (entry.accumulator >= PhysicsConstants::c_FixedTimeStep && steps < PhysicsConstants::c_MaxStepsPerFrame)
		{
			Step(data, world);
			entry.accumulator -= PhysicsConstants::c_FixedTimeStep;
			++steps;
		}

		// Time it couldn't catch up on is dropped rather than carried into later frames.
		if (entry.accumulator >= PhysicsConstants::c_FixedTimeStep)
		{
			entry.accumulator = 0.0f;
		}
	}

	float PhysicsManager::GetInterpolation(Handle physicsWorld) const
	{
		return m_Worlds[physicsWorld].accumulator / PhysicsConstants::c_FixedTimeStep;
	}

	void PhysicsManager::RemoveBody(Handle physicsWorld, ColliderComponent& collider)
	{
		if (collider.bodyID == PhysicsConstants::c_InvalidBodyID)
		{
			return;
		}

		JPH::BodyInterface& bodies = m_Worlds[physicsWorld].data->system.GetBodyInterfaceNoLock();
		const JPH::BodyID bodyID(collider.bodyID);
		bodies.RemoveBody(bodyID);
		bodies.DestroyBody(bodyID);
		collider.bodyID = PhysicsConstants::c_InvalidBodyID;
	}

	bool PhysicsManager::Raycast(Handle physicsWorld, const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit) const
	{
		const PhysicsWorldData& data = *m_Worlds[physicsWorld].data;
		const JPH::RRayCast ray(ToJolt(origin), ToJolt(Vector3::Normalize(direction) * maxDistance));

		JPH::RayCastResult result;
		if (!data.system.GetNarrowPhaseQuery().CastRay(ray, result))
		{
			return false;
		}

		const JPH::Vec3 point = ray.GetPointOnRay(result.mFraction);
		outHit.position = FromJolt(point);
		outHit.distance = result.mFraction * maxDistance;

		JPH::BodyLockRead lock(data.system.GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded())
		{
			const JPH::Body& body = lock.GetBody();
			outHit.entity = static_cast<Entity>(body.GetUserData());
			outHit.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
		}
		return true;
	}

	void PhysicsManager::CreateBodies(PhysicsWorldData& data, World& world, const AssetManager& assetManager)
	{
		JPH::BodyInterface& bodies = data.system.GetBodyInterfaceNoLock();
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
			const bool fromMesh = colliderShape == ColliderShape::Mesh || colliderShape == ColliderShape::ConvexHull;
			const JPH::ShapeRefC shape = fromMesh ? CreateMeshShape(collider, transform.scale, motionType, assetManager) : CreateShape(collider, transform.scale);
			if (!shape)
			{
				return;
			}

			const bool moves = motionType != MotionType::Static;
			JPH::BodyCreationSettings settings(shape, ToJolt(transform.position), ToJolt(transform.rotation),
				moves ? (motionType == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic) : JPH::EMotionType::Static,
				moves ? ObjectLayers::c_Moving : ObjectLayers::c_NonMoving);
			settings.mFriction = collider.friction;
			settings.mRestitution = collider.restitution;
			settings.mUserData = static_cast<JPH::uint64>(entity);
			if (rigidBody)
			{
				settings.mLinearDamping = rigidBody->linearDamping;
				settings.mAngularDamping = rigidBody->angularDamping;
				settings.mGravityFactor = rigidBody->gravityFactor;
				if (rigidBody->mass > 0.0f)
				{
					settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
					settings.mMassPropertiesOverride.mMass = rigidBody->mass;
				}
				// A triangle mesh has no volume to work its mass out from. Only a kinematic one gets here.
				if (moves && colliderShape == ColliderShape::Mesh && motionType != MotionType::Dynamic)
				{
					const JPH::Vec3 size = JPH::Vec3::sMax(shape->GetLocalBounds().GetSize(), JPH::Vec3::sReplicate(0.1f));
					settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
					settings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(size, 1000.0f);
				}

				rigidBody->previousPosition = transform.position;
				rigidBody->previousRotation = transform.rotation;
				rigidBody->currentPosition = transform.position;
				rigidBody->currentRotation = transform.rotation;
			}

			const JPH::BodyID bodyID = bodies.CreateAndAddBody(settings, moves ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
			if (bodyID.IsInvalid())
			{
				TYR_LOG_ERROR("Out of physics bodies.");
				return;
			}
			collider.bodyID = bodyID.GetIndexAndSequenceNumber();
			addedAny = true;
		});

		// Keeps broad phase queries fast after adding a lot of bodies at once, such as on level load.
		if (addedAny)
		{
			data.system.OptimizeBroadPhase();
		}
	}

	void PhysicsManager::Step(PhysicsWorldData& data, World& world)
	{
		JPH::BodyInterface& bodies = data.system.GetBodyInterfaceNoLock();

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
				bodies.MoveKinematic(JPH::BodyID(collider.bodyID), ToJolt(transform.position), ToJolt(transform.rotation), PhysicsConstants::c_FixedTimeStep);
			}
		});

		data.system.Update(PhysicsConstants::c_FixedTimeStep, 1, m_TempAllocator, m_JobSystem);

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

			JPH::RVec3 position;
			JPH::Quat rotation;
			bodies.GetPositionAndRotation(JPH::BodyID(collider.bodyID), position, rotation);
			rigidBody.previousPosition = rigidBody.currentPosition;
			rigidBody.previousRotation = rigidBody.currentRotation;
			rigidBody.currentPosition = FromJolt(position);
			rigidBody.currentRotation = FromJolt(rotation);
		});
	}
}
