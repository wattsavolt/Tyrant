#include "PhysicsScene.h"
#include "PhysicsContext.h"
#include "Private/PhysicsContextData.h"
#include "Private/PhysicsJobSystem.h"
#include "Private/JoltUtil.h"
#include "Math/Math.h"

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace tyr
{
	using namespace JoltUtil;

	namespace
	{
		// Jolt's recommended mutex count for most body counts.
		constexpr JPH::uint c_BodyMutexCount = 0;

		namespace ObjectLayers
		{
			constexpr JPH::ObjectLayer c_NonMoving = 0;
			constexpr JPH::ObjectLayer c_Moving = 1;
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

		// A capsule standing on the origin, so the character's position is its base.
		JPH::RefConst<JPH::Shape> CreateCharacterShape(float radius, float height)
		{
			const float safeRadius = radius > 0.05f ? radius : 0.05f;
			const float halfCylinder = height * 0.5f - safeRadius > 0.01f ? height * 0.5f - safeRadius : 0.01f;
			const JPH::Vec3 offset(0.0f, halfCylinder + safeRadius, 0.0f);
			return JPH::RotatedTranslatedShapeSettings(offset, JPH::Quat::sIdentity(), new JPH::CapsuleShape(halfCylinder, safeRadius)).Create().Get();
		}
	}

	// A character, with a shape for each stance.
	struct PhysicsCharacter
	{
		JPH::Ref<JPH::CharacterVirtual> character;
		JPH::RefConst<JPH::Shape> standingShape;
		JPH::RefConst<JPH::Shape> crouchingShape;
		bool crouched = false;
	};

	struct PhysicsSceneData
	{
		BroadPhaseLayerMap broadPhaseLayers;
		ObjectVsBroadPhaseFilter objectVsBroadPhaseFilter;
		ObjectPairFilter objectPairFilter;
		JPH::PhysicsSystem system;
		// After the system, so they're destroyed before it. Empty slots have no character.
		PhysicsCharacter characters[PhysicsConstants::c_MaxCharacters];
	};

	PhysicsScene::PhysicsScene(PhysicsContext& context)
		: m_Context(context)
	{
		// Made when a game world starts and kept until it ends.
		m_Data = new PhysicsSceneData();
		m_Data->system.Init(PhysicsConstants::c_MaxBodies, c_BodyMutexCount, PhysicsConstants::c_MaxBodyPairs, PhysicsConstants::c_MaxContactConstraints,
			m_Data->broadPhaseLayers, m_Data->objectVsBroadPhaseFilter, m_Data->objectPairFilter);
		m_Data->system.SetGravity(JPH::Vec3(0.0f, PhysicsConstants::c_Gravity, 0.0f));
	}

	PhysicsScene::~PhysicsScene()
	{
		JPH::BodyInterface& bodies = m_Data->system.GetBodyInterfaceNoLock();
		JPH::BodyIDVector bodyIDs;
		m_Data->system.GetBodies(bodyIDs);
		if (!bodyIDs.empty())
		{
			bodies.RemoveBodies(bodyIDs.data(), static_cast<int>(bodyIDs.size()));
			bodies.DestroyBodies(bodyIDs.data(), static_cast<int>(bodyIDs.size()));
		}
		delete m_Data;
		m_Data = nullptr;
	}

	uint PhysicsScene::AddBody(const BodyDesc& desc)
	{
		const JPH::Shape* shape = ToJolt(desc.shape);
		if (!shape)
		{
			return PhysicsConstants::c_InvalidBodyID;
		}

		const bool moves = desc.motion != MotionType::Static;
		const JPH::EMotionType motionType = desc.motion == MotionType::Dynamic ? JPH::EMotionType::Dynamic
			: (desc.motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Static);
		JPH::BodyCreationSettings settings(shape, ToJolt(desc.position), ToJolt(desc.rotation), motionType,
			moves ? ObjectLayers::c_Moving : ObjectLayers::c_NonMoving);
		settings.mFriction = desc.friction;
		settings.mRestitution = desc.restitution;
		settings.mLinearDamping = desc.linearDamping;
		settings.mAngularDamping = desc.angularDamping;
		settings.mGravityFactor = desc.gravityFactor;
		settings.mUserData = desc.userData;
		if (desc.mass > 0.0f)
		{
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = desc.mass;
		}
		if (desc.massFromBounds)
		{
			const JPH::Vec3 size = JPH::Vec3::sMax(shape->GetLocalBounds().GetSize(), JPH::Vec3::sReplicate(0.1f));
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
			settings.mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(size, 1000.0f);
		}

		const JPH::BodyID bodyID = m_Data->system.GetBodyInterfaceNoLock().CreateAndAddBody(settings, moves ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
		return bodyID.IsInvalid() ? PhysicsConstants::c_InvalidBodyID : bodyID.GetIndexAndSequenceNumber();
	}

	void PhysicsScene::RemoveBody(uint body)
	{
		if (body == PhysicsConstants::c_InvalidBodyID)
		{
			return;
		}
		JPH::BodyInterface& bodies = m_Data->system.GetBodyInterfaceNoLock();
		const JPH::BodyID bodyID(body);
		bodies.RemoveBody(bodyID);
		bodies.DestroyBody(bodyID);
	}

	void PhysicsScene::OptimizeBroadPhase()
	{
		m_Data->system.OptimizeBroadPhase();
	}

	void PhysicsScene::MoveKinematic(uint body, const Vector3& position, const Quaternion& rotation, float deltaTime)
	{
		m_Data->system.GetBodyInterfaceNoLock().MoveKinematic(JPH::BodyID(body), ToJolt(position), ToJolt(rotation), deltaTime);
	}

	void PhysicsScene::GetBodyTransform(uint body, Vector3& outPosition, Quaternion& outRotation) const
	{
		JPH::RVec3 position;
		JPH::Quat rotation;
		m_Data->system.GetBodyInterfaceNoLock().GetPositionAndRotation(JPH::BodyID(body), position, rotation);
		outPosition = FromJolt(position);
		outRotation = FromJolt(rotation);
	}

	void PhysicsScene::Step(float deltaTime)
	{
		const PhysicsContextData& context = m_Context.GetData();
		m_Data->system.Update(deltaTime, 1, context.tempAllocator, context.jobSystem);
	}

	bool PhysicsScene::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, PhysicsRayHit& outHit) const
	{
		const JPH::RRayCast ray(ToJolt(origin), ToJolt(Vector3::Normalize(direction) * maxDistance));
		JPH::RayCastResult result;
		if (!m_Data->system.GetNarrowPhaseQuery().CastRay(ray, result))
		{
			return false;
		}

		const JPH::Vec3 point = ray.GetPointOnRay(result.mFraction);
		outHit.position = FromJolt(point);
		outHit.distance = result.mFraction * maxDistance;

		JPH::BodyLockRead lock(m_Data->system.GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded())
		{
			const JPH::Body& body = lock.GetBody();
			outHit.userData = body.GetUserData();
			outHit.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
		}
		return true;
	}

	uint PhysicsScene::AddCharacter(const CharacterDesc& desc)
	{
		uint slot = 0;
		while (slot < PhysicsConstants::c_MaxCharacters && m_Data->characters[slot].character != nullptr)
		{
			++slot;
		}
		if (slot == PhysicsConstants::c_MaxCharacters)
		{
			return PhysicsConstants::c_InvalidCharacter;
		}

		PhysicsCharacter& physicsCharacter = m_Data->characters[slot];
		physicsCharacter.standingShape = CreateCharacterShape(desc.radius, desc.height);
		physicsCharacter.crouchingShape = CreateCharacterShape(desc.radius, desc.crouchHeight);
		physicsCharacter.crouched = false;

		JPH::CharacterVirtualSettings settings;
		settings.mShape = physicsCharacter.standingShape;
		settings.mMaxSlopeAngle = desc.maxSlope * Math::c_DegToRad;
		// Contacts below the centre of the capsule's bottom sphere hold it up.
		settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -desc.radius);
		// Mesh colliders have internal edges it would otherwise catch on.
		settings.mEnhancedInternalEdgeRemoval = true;

		physicsCharacter.character = new JPH::CharacterVirtual(&settings, ToJolt(desc.position), JPH::Quat::sIdentity(), desc.userData, &m_Data->system);
		return slot;
	}

	void PhysicsScene::RemoveCharacter(uint character)
	{
		if (character < PhysicsConstants::c_MaxCharacters)
		{
			m_Data->characters[character] = PhysicsCharacter();
		}
	}

	CharacterState PhysicsScene::MoveCharacter(uint character, const CharacterMove& move, float deltaTime)
	{
		PhysicsCharacter& physicsCharacter = m_Data->characters[character];
		JPH::CharacterVirtual& virtualCharacter = *physicsCharacter.character;
		JPH::TempAllocator& tempAllocator = *m_Context.GetData().tempAllocator;

		const JPH::Vec3 gravity = m_Data->system.GetGravity();
		const JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter = m_Data->system.GetDefaultBroadPhaseLayerFilter(ObjectLayers::c_Moving);
		const JPH::DefaultObjectLayerFilter layerFilter = m_Data->system.GetDefaultLayerFilter(ObjectLayers::c_Moving);
		const JPH::BodyFilter bodyFilter;
		const JPH::ShapeFilter shapeFilter;

		// Standing back up only happens once the taller capsule fits.
		if (move.wantsCrouch != physicsCharacter.crouched)
		{
			const JPH::Shape* shape = move.wantsCrouch ? physicsCharacter.crouchingShape.GetPtr() : physicsCharacter.standingShape.GetPtr();
			if (virtualCharacter.SetShape(shape, 1.5f * virtualCharacter.GetCharacterPadding(), broadPhaseFilter, layerFilter, bodyFilter, shapeFilter, tempAllocator))
			{
				physicsCharacter.crouched = move.wantsCrouch;
			}
		}

		// On the ground it moves with whatever it stands on, otherwise it keeps falling.
		virtualCharacter.UpdateGroundVelocity();
		const JPH::Vec3 up = virtualCharacter.GetUp();
		const float verticalSpeed = virtualCharacter.GetLinearVelocity().Dot(up);
		const JPH::Vec3 groundVelocity = virtualCharacter.GetGroundVelocity();
		const bool onGround = virtualCharacter.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround && verticalSpeed - groundVelocity.Dot(up) < 0.1f;
		JPH::Vec3 velocity = onGround ? groundVelocity : up * verticalSpeed;
		velocity += gravity * deltaTime + ToJolt(move.moveVelocity);
		virtualCharacter.SetLinearVelocity(velocity);

		JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
		updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, -move.stepHeight, 0.0f);
		updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, move.stepHeight, 0.0f);
		virtualCharacter.ExtendedUpdate(deltaTime, gravity, updateSettings, broadPhaseFilter, layerFilter, bodyFilter, shapeFilter, tempAllocator);

		CharacterState state;
		state.position = FromJolt(virtualCharacter.GetPosition());
		state.grounded = virtualCharacter.IsSupported();
		state.crouched = physicsCharacter.crouched;
		return state;
	}
}
