#include "LimbCollision.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Animation/AnimationSystem.h"
#include "Gameplay/Doors.h"
#include "Math/Math.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// How quickly a limb sunk into a wall is drawn back out, as a share of the depth per second.
		constexpr float c_PushRate = 12.0f;
		// Which arm, if any, each bone belongs to.
		constexpr uint8 c_NoArm = 0xFF;

		// The furthest push each way along x and z, so overlapping limbs don't add up.
		struct PushBounds
		{
			Vector3 positive = Vector3::c_Zero;
			Vector3 negative = Vector3::c_Zero;

			void Add(const Vector3& push)
			{
				positive = Vector3(std::max(positive.x, push.x), 0.0f, std::max(positive.z, push.z));
				negative = Vector3(std::min(negative.x, push.x), 0.0f, std::min(negative.z, push.z));
			}

			Vector3 Total() const
			{
				return positive + negative;
			}
		};

		Vector3 ToWorld(const Transform& body, const Vector3& point)
		{
			return body.position + body.rotation.Rotate(Vector3(point.x * body.scale.x, point.y * body.scale.y, point.z * body.scale.z));
		}
	}

	namespace LimbCollision
	{
		void Update(World& world, const PhysicsComponentsManager& physics, AnimationSystem& animation, float deltaTime)
		{
			world.entities.ForEach<LimbCollisionComponent>([&](Entity entity, LimbCollisionComponent& limbs)
			{
				if (!world.entities.HasComponent<SkeletalMeshComponent>(entity) || !world.entities.HasComponent<CharacterComponent>(entity)
					|| !world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				// Held still by the game, such as crouched over a body its limbs reach into, or held in place, it's left where it is.
				const bool holding = world.entities.HasComponent<NavAgentComponent>(entity) && world.entities.GetComponent<NavAgentComponent>(entity).holdTimer > 0.0f;
				if (limbs.still || holding)
				{
					limbs.armTurns[0] = Quaternion::c_Identity;
					limbs.armTurns[1] = Quaternion::c_Identity;
					return;
				}
				const AnimatorComponent* animator = world.entities.HasComponent<AnimatorComponent>(entity) ? &world.entities.GetComponent<AnimatorComponent>(entity) : nullptr;
				const Matrix4* bones = nullptr;
				const Skeleton* skeleton = nullptr;
				const uint boneCount = animation.GetModelPose(world.entities.GetComponent<SkeletalMeshComponent>(entity), animator, bones, skeleton);
				if (boneCount == 0)
				{
					return;
				}

				// The door it's bashing on only stops its arms, so it can stand close enough to hit it.
				Entity bashed = c_InvalidEntity;
				Entity bashedLinked = c_InvalidEntity;
				if (world.entities.HasComponent<NavAgentComponent>(entity))
				{
					bashed = world.entities.GetComponent<NavAgentComponent>(entity).bashDoor;
					if (bashed != c_InvalidEntity && world.entities.HasComponent<DoorComponent>(bashed))
					{
						bashedLinked = Doors::Find(world, world.entities.GetComponent<DoorComponent>(bashed).linkedDoor);
					}
				}

				const BoneName* const armNames[2] = { &limbs.leftArm, &limbs.rightArm };
				uint8 arm[SkeletonConstants::c_MaxBones];
				for (uint side = 0; side < 2; ++side)
				{
					if (limbs.armBones[side] < 0 && armNames[side]->Size() > 0)
					{
						const uint16 found = skeleton->FindBone(armNames[side]->CStr());
						limbs.armBones[side] = found == SkeletonConstants::c_NoParent ? -1 : static_cast<int>(found);
					}
				}
				// Bones are stored parents first, so each takes its parent's arm.
				for (uint bone = 0; bone < boneCount; ++bone)
				{
					const uint16 parent = skeleton->parents[bone];
					arm[bone] = parent == SkeletonConstants::c_NoParent ? c_NoArm : arm[parent];
					for (uint8 side = 0; side < 2; ++side)
					{
						arm[bone] = limbs.armBones[side] == static_cast<int>(bone) ? side : arm[bone];
					}
				}

				const Transform& body = world.entities.GetComponent<TransformComponent>(entity).world;
				const float minLengthSquared = limbs.minBoneLength * limbs.minBoneLength;
				PushBounds bodyPush;
				PushBounds armPush[2];
				// Each arm swings about its shoulder towards wherever its furthest bone is pushed.
				float armReach[2] = { 0.0f, 0.0f };
				Vector3 armTip[2] = { Vector3::c_Zero, Vector3::c_Zero };
				for (uint bone = 0; bone < boneCount; ++bone)
				{
					const uint16 parent = skeleton->parents[bone];
					if (parent == SkeletonConstants::c_NoParent)
					{
						continue;
					}
					const Vector3 from = bones[parent].GetTranslation();
					const Vector3 to = bones[bone].GetTranslation();
					if ((to - from).SqrLength() < minLengthSquared)
					{
						continue;
					}
					const Vector3 a = ToWorld(body, from);
					const Vector3 b = ToWorld(body, to);
					const uint8 side = arm[bone] != c_NoArm && arm[parent] == arm[bone] ? arm[bone] : c_NoArm;
					if (side == c_NoArm)
					{
						bodyPush.Add(physics.FindCapsulePush(world.physicsWorld, a, b, limbs.limbRadius, true, &world, bashed, bashedLinked));
						continue;
					}
					armPush[side].Add(physics.FindCapsulePush(world.physicsWorld, a, b, limbs.limbRadius, true));
					const float reach = (to - bones[limbs.armBones[side]].GetTranslation()).SqrLength();
					if (reach > armReach[side])
					{
						armReach[side] = reach;
						armTip[side] = b;
					}
				}

				// An arm in the way of something turns at the shoulder just far enough for its tip to clear it.
				const float blend = 1.0f - Math::Exp(-limbs.armTurnSpeed * deltaTime);
				const float maxTurnCos = Math::Cos(limbs.maxArmTurn * Math::c_DegToRad);
				for (uint side = 0; side < 2; ++side)
				{
					Quaternion wanted = Quaternion::c_Identity;
					const Vector3 push = armPush[side].Total();
					if (limbs.armBones[side] >= 0 && armReach[side] > 0.0f && push.SqrLength() > 1.0e-8f)
					{
						const Vector3 shoulder = ToWorld(body, bones[limbs.armBones[side]].GetTranslation());
						Vector3 reach = armTip[side] - shoulder;
						Vector3 moved = reach + push;
						reach.SafeNormalize();
						moved.SafeNormalize();
						if (reach.Dot(moved) < maxTurnCos)
						{
							// No further than the most it may turn: part of the way round the same arc.
							const float share = Math::Acos(maxTurnCos) / std::max(Math::Acos(Math::Clamp(reach.Dot(moved), -1.0f, 1.0f)), 1.0e-4f);
							moved = Vector3::Normalize(reach + (moved - reach) * share);
						}
						// Worked out in the world, then turned into the mesh's own space.
						const Quaternion turn = Quaternion::GetRotationBetweenVectors(reach, moved);
						wanted = body.rotation.Inverse() * turn * body.rotation;
					}
					limbs.armTurns[side] = Quaternion::Slerp(blend, limbs.armTurns[side], wanted);
				}

				Vector3 velocity = bodyPush.Total() * c_PushRate;
				const float speed = velocity.Length();
				if (speed > limbs.maxPushSpeed)
				{
					velocity *= limbs.maxPushSpeed / speed;
				}
				world.entities.GetComponent<CharacterComponent>(entity).moveVelocity += velocity;
			});
		}
	}
}
