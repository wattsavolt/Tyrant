#include "AnimationSystem.h"
#include "AnimationUtil.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/MeshAsset.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Rendering/RendererAPI.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		constexpr float c_NoEnd = 1e30f;

		// Eases a fade in and out, so a blend never starts or stops with a jolt.
		float SmoothFade(float fade)
		{
			const float t = Math::Clamp01(fade);
			return t * t * (3.0f - 2.0f * t);
		}
	}

	void AnimationSystem::Initialize(AssetManager& assetManager, RendererAPI& rendererAPI)
	{
		m_AssetManager = &assetManager;
		m_RendererAPI = &rendererAPI;
	}

	void AnimationSystem::Update(World& world, float deltaTime)
	{
		world.entities.ForEach<SkeletalMeshComponent>([this, &world, deltaTime](Entity entity, SkeletalMeshComponent& skeletalMesh)
		{
			AnimatorComponent* animator = nullptr;
			if (world.entities.HasComponent<AnimatorComponent>(entity))
			{
				animator = &world.entities.GetComponent<AnimatorComponent>(entity);
				SyncClips(*animator, world.simulate);
				if (world.simulate)
				{
					Advance(*animator, deltaTime);
				}
			}

			if (skeletalMesh.meshInstance)
			{
				const LimbCollisionComponent* limbs = world.entities.HasComponent<LimbCollisionComponent>(entity)
					? &world.entities.GetComponent<LimbCollisionComponent>(entity) : nullptr;
				PoseInstance(skeletalMesh, animator, limbs);
			}
		});
	}

	void AnimationSystem::Play(AnimatorComponent& animator, AssetID clip, const AnimationPlayDesc& desc)
	{
		if (!AssetUtil::IsValidAssetID(clip))
		{
			return;
		}
		AnimationPlayback* newest = animator.blends.IsEmpty() ? nullptr : &animator.blends[animator.blends.Size() - 1];
		if (newest && newest->clip == clip && !desc.restart)
		{
			newest->loop = desc.loop;
			newest->inPlace = desc.inPlace;
			newest->playRate = desc.playRate;
			return;
		}

		AnimationPlayback playback;
		playback.playRate = desc.playRate;
		playback.loop = desc.loop;
		playback.inPlace = desc.inPlace;
		playback.fadeDuration = desc.fadeDuration;
		playback.fade = desc.fadeDuration > 0.0f && newest ? 0.0f : 1.0f;
		if (desc.matchPhase && newest)
		{
			const AnimationClip* replaced = m_AssetManager->GetAnimation(newest->clip);
			if (replaced && replaced->duration > 0.0f)
			{
				playback.startPhase = newest->time / replaced->duration;
			}
		}

		// Nothing under a clip that's fully in shows, so a cut drops the rest. When there's no
		// room the oldest goes, since it shows least.
		if (playback.fade >= 1.0f)
		{
			for (AnimationPlayback& old : animator.blends)
			{
				SetPlaybackClip(old, AssetConstants::c_InvalidAssetID);
			}
			animator.blends.Clear();
		}
		else if (animator.blends.Size() == c_MaxBlendedClips)
		{
			SetPlaybackClip(animator.blends[0], AssetConstants::c_InvalidAssetID);
			animator.blends.Erase(0);
			animator.blends[0].fade = 1.0f;
		}

		SetPlaybackClip(playback, clip);
		animator.blends.Add(playback);
		animator.clip = clip;
	}

	void AnimationSystem::Play(AnimatorComponent& animator, AssetID clip, float fadeDuration)
	{
		AnimationPlayDesc desc;
		desc.fadeDuration = fadeDuration;
		desc.loop = animator.loop;
		Play(animator, clip, desc);
	}

	void AnimationSystem::SetPlayRate(AnimatorComponent& animator, float playRate)
	{
		if (!animator.blends.IsEmpty())
		{
			animator.blends[animator.blends.Size() - 1].playRate = playRate;
		}
	}

	float AnimationSystem::GetTimeLeft(const AnimatorComponent& animator) const
	{
		if (animator.blends.IsEmpty())
		{
			return c_NoEnd;
		}
		const AnimationPlayback& newest = animator.blends[animator.blends.Size() - 1];
		const AnimationClip* clip = m_AssetManager->GetAnimation(newest.clip);
		const float rate = newest.playRate * animator.playRate;
		if (!clip || newest.loop || rate <= 0.0f || newest.startPhase >= 0.0f)
		{
			return c_NoEnd;
		}
		return (clip->duration - newest.time) / rate;
	}

	float AnimationSystem::GetGroundSpeed(const AnimatorComponent& animator) const
	{
		if (animator.blends.IsEmpty())
		{
			return 0.0f;
		}
		const AnimationClip* clip = m_AssetManager->GetAnimation(animator.blends[animator.blends.Size() - 1].clip);
		if (!clip || clip->duration <= 0.0f)
		{
			return 0.0f;
		}
		const Vector3 travel = AnimationUtil::GetRootTravel(*clip);
		return Vector3(travel.x, 0.0f, travel.z).Length() / clip->duration;
	}

	void AnimationSystem::Release(AnimatorComponent& animator)
	{
		for (AnimationPlayback& playback : animator.blends)
		{
			SetPlaybackClip(playback, AssetConstants::c_InvalidAssetID);
		}
		animator.blends.Clear();
		SetPlaybackClip(animator.layer, AssetConstants::c_InvalidAssetID);
	}

	void AnimationSystem::SyncClips(AnimatorComponent& animator, bool simulate)
	{
		const AssetID newest = animator.blends.IsEmpty() ? AssetConstants::c_InvalidAssetID : animator.blends[animator.blends.Size() - 1].clip;
		if (!(animator.clip == newest))
		{
			if (AssetUtil::IsValidAssetID(animator.clip))
			{
				// A world that isn't simulated never moves a fade on, so it switches straight over.
				AnimationPlayDesc desc;
				desc.fadeDuration = simulate ? animator.fadeTime : 0.0f;
				desc.loop = animator.loop;
				Play(animator, animator.clip, desc);
			}
			else
			{
				for (AnimationPlayback& playback : animator.blends)
				{
					SetPlaybackClip(playback, AssetConstants::c_InvalidAssetID);
				}
				animator.blends.Clear();
			}
		}

		if (!(animator.layerClip == animator.layer.clip))
		{
			SetPlaybackClip(animator.layer, animator.layerClip);
			animator.layer.time = 0.0f;
		}
	}

	void AnimationSystem::SetPlaybackClip(AnimationPlayback& playback, AssetID clip)
	{
		// The new clip is loaded first, in case it's the same one.
		if (AssetUtil::IsValidAssetID(clip))
		{
			m_AssetManager->LoadAnimation(clip);
		}
		if (AssetUtil::IsValidAssetID(playback.clip))
		{
			m_AssetManager->DeleteAnimation(playback.clip);
		}
		playback.clip = clip;
	}

	void AnimationSystem::Advance(AnimatorComponent& animator, float deltaTime)
	{
		for (AnimationPlayback& playback : animator.blends)
		{
			// A clip still loading stays at its start.
			const AnimationClip* clip = m_AssetManager->GetAnimation(playback.clip);
			if (!clip)
			{
				continue;
			}
			if (playback.startPhase >= 0.0f)
			{
				playback.time = AnimationUtil::WrapTime(playback.startPhase * clip->duration, clip->duration, playback.loop);
				playback.startPhase = -1.0f;
			}
			else
			{
				playback.time = AnimationUtil::WrapTime(playback.time + deltaTime * playback.playRate * animator.playRate, clip->duration, playback.loop);
			}
		}

		for (AnimationPlayback& playback : animator.blends)
		{
			if (playback.fade < 1.0f)
			{
				playback.fade = playback.fadeDuration > 0.0f ? std::min(playback.fade + deltaTime / playback.fadeDuration, 1.0f) : 1.0f;
			}
		}

		// Clips under one that has fully faded in no longer show.
		for (uint index = animator.blends.Size(); index-- > 1;)
		{
			if (animator.blends[index].fade >= 1.0f)
			{
				for (uint old = 0; old < index; ++old)
				{
					SetPlaybackClip(animator.blends[old], AssetConstants::c_InvalidAssetID);
				}
				for (uint old = 0; old < index; ++old)
				{
					animator.blends.Erase(0);
				}
				break;
			}
		}

		if (const AnimationClip* clip = m_AssetManager->GetAnimation(animator.layer.clip))
		{
			animator.layer.time = AnimationUtil::WrapTime(animator.layer.time + deltaTime * animator.playRate, clip->duration, true);
		}
	}

	const AnimationClip* AnimationSystem::GetUsableClip(AssetID clip, AssetID skeletonID, const Skeleton& skeleton) const
	{
		const AnimationClip* animation = m_AssetManager->GetAnimation(clip);
		if (!animation || !(animation->skeleton == skeletonID) || animation->tracks.Size() != skeleton.BoneCount())
		{
			return nullptr;
		}
		return animation;
	}

	void AnimationSystem::SamplePlayback(const AnimationClip& clip, const Skeleton& skeleton, const AnimationPlayback& playback, BoneTransform* outPose) const
	{
		const float time = playback.startPhase >= 0.0f ? 0.0f : playback.time;
		AnimationUtil::SampleClip(clip, skeleton, time, outPose);
		if (playback.inPlace)
		{
			AnimationUtil::RemoveRootTravel(clip, skeleton, outPose);
		}
	}

	void AnimationSystem::PoseInstance(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, const LimbCollisionComponent* limbs)
	{
		const uint boneCount = ComputePose(skeletalMesh, animator);
		if (boneCount == 0)
		{
			return;
		}
		const MeshHeader* header = m_AssetManager->GetMeshHeader(skeletalMesh.mesh);
		const Skeleton* skeleton = m_AssetManager->GetSkeleton(header->skeleton);
		if (limbs)
		{
			TurnArms(*skeleton, *limbs, boneCount);
		}
		AnimationUtil::ComputeSkinMatrices(*skeleton, m_ModelMatrices, m_SkinMatrices);
		const BoundingSphere bounds = AnimationUtil::ComputePoseBounds(m_ModelMatrices, boneCount, header->boneBoundsMargin);
		m_RendererAPI->UpdateSkeletalMeshPose(skeletalMesh.meshInstance, m_SkinMatrices, boneCount, bounds);
	}

	void AnimationSystem::TurnArms(const Skeleton& skeleton, const LimbCollisionComponent& limbs, uint boneCount)
	{
		for (uint side = 0; side < 2; ++side)
		{
			const int shoulder = limbs.armBones[side];
			if (shoulder < 0 || shoulder >= static_cast<int>(boneCount) || limbs.armTurns[side].w > 0.99999f)
			{
				continue;
			}
			// The whole arm turns about the shoulder: the shoulder bone and everything under it.
			const Vector3 pivot = m_ModelMatrices[shoulder].GetTranslation();
			const Matrix4 turn = Matrix4::CreateTranslation(-pivot) * Matrix4::CreateRotation(limbs.armTurns[side]) * Matrix4::CreateTranslation(pivot);
			bool inArm[SkeletonConstants::c_MaxBones];
			for (uint bone = 0; bone < boneCount; ++bone)
			{
				const uint16 parent = skeleton.parents[bone];
				inArm[bone] = static_cast<int>(bone) == shoulder || (parent != SkeletonConstants::c_NoParent && inArm[parent]);
				if (inArm[bone])
				{
					m_ModelMatrices[bone] = m_ModelMatrices[bone] * turn;
				}
			}
		}
	}

	uint AnimationSystem::GetBoneCount(const SkeletalMeshComponent& skeletalMesh) const
	{
		const MeshHeader* header = m_AssetManager->GetMeshHeader(skeletalMesh.mesh);
		const Skeleton* skeleton = header ? m_AssetManager->GetSkeleton(header->skeleton) : nullptr;
		return skeleton ? skeleton->BoneCount() : 0;
	}

	bool AnimationSystem::GetBonePosition(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, uint bone, Vector3& outPosition)
	{
		if (bone >= ComputePose(skeletalMesh, animator))
		{
			return false;
		}
		outPosition = m_ModelMatrices[bone].GetTranslation();
		return true;
	}

	uint AnimationSystem::GetModelPose(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, const Matrix4*& outMatrices, const Skeleton*& outSkeleton)
	{
		const uint boneCount = ComputePose(skeletalMesh, animator);
		if (boneCount == 0)
		{
			return 0;
		}
		outMatrices = m_ModelMatrices;
		outSkeleton = m_AssetManager->GetSkeleton(m_AssetManager->GetMeshHeader(skeletalMesh.mesh)->skeleton);
		return boneCount;
	}

	uint AnimationSystem::ComputePose(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator)
	{
		const MeshHeader* header = m_AssetManager->GetMeshHeader(skeletalMesh.mesh);
		const Skeleton* skeleton = header ? m_AssetManager->GetSkeleton(header->skeleton) : nullptr;
		if (!skeleton)
		{
			return 0;
		}

		// Each clip blends in over the pose of the ones under it, by how far it has faded in.
		const uint boneCount = skeleton->BoneCount();
		memcpy(m_Pose, skeleton->bindPose.Data(), sizeof(BoneTransform) * boneCount);
		if (animator)
		{
			bool posed = false;
			for (const AnimationPlayback& playback : animator->blends)
			{
				const AnimationClip* clip = GetUsableClip(playback.clip, header->skeleton, *skeleton);
				if (!clip)
				{
					continue;
				}
				if (!posed)
				{
					SamplePlayback(*clip, *skeleton, playback, m_Pose);
					posed = true;
					continue;
				}
				SamplePlayback(*clip, *skeleton, playback, m_BlendPose);
				AnimationUtil::BlendPoses(m_Pose, m_BlendPose, SmoothFade(playback.fade), boneCount, m_Pose);
			}
		}

		if (animator && animator->layerWeight > 0.0f && animator->layerMaskBone < boneCount)
		{
			if (const AnimationClip* layer = GetUsableClip(animator->layer.clip, header->skeleton, *skeleton))
			{
				AnimationUtil::SampleClip(*layer, *skeleton, animator->layer.time, m_BlendPose);
				AnimationUtil::BuildBoneMask(*skeleton, static_cast<uint16>(animator->layerMaskBone), Math::Clamp01(animator->layerWeight), m_BoneWeights);
				AnimationUtil::BlendPosesPerBone(m_Pose, m_BlendPose, m_BoneWeights, boneCount, m_Pose);
			}
		}

		AnimationUtil::ComputeModelMatrices(*skeleton, m_Pose, m_ModelMatrices);
		return boneCount;
	}
}
