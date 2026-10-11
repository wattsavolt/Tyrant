#include "AnimationSystem.h"
#include "AnimationUtil.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/MeshAsset.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Rendering/RendererAPI.h"

namespace tyr
{
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
				PoseInstance(skeletalMesh, animator);
			}
		});
	}

	void AnimationSystem::Play(AnimatorComponent& animator, AssetID clip, float fadeDuration)
	{
		if (clip == animator.current.clip)
		{
			return;
		}

		// The clip being left keeps playing from where it was while it fades out.
		if (fadeDuration > 0.0f && AssetUtil::IsValidAssetID(animator.current.clip))
		{
			SetPlaybackClip(animator.previous, animator.current.clip);
			animator.previous.time = animator.current.time;
			animator.fadeDuration = fadeDuration;
			animator.fadeElapsed = 0.0f;
		}
		else
		{
			SetPlaybackClip(animator.previous, AssetConstants::c_InvalidAssetID);
			animator.fadeDuration = 0.0f;
		}

		SetPlaybackClip(animator.current, clip);
		animator.current.time = 0.0f;
		animator.clip = clip;
	}

	void AnimationSystem::Release(AnimatorComponent& animator)
	{
		SetPlaybackClip(animator.current, AssetConstants::c_InvalidAssetID);
		SetPlaybackClip(animator.previous, AssetConstants::c_InvalidAssetID);
		SetPlaybackClip(animator.layer, AssetConstants::c_InvalidAssetID);
		animator.fadeDuration = 0.0f;
	}

	void AnimationSystem::SyncClips(AnimatorComponent& animator, bool simulate)
	{
		// A world that isn't simulated never moves a fade on, so it switches straight over.
		if (simulate && !(animator.clip == animator.current.clip))
		{
			Play(animator, animator.clip, animator.fadeTime);
		}
		else if (!(animator.clip == animator.current.clip))
		{
			SetPlaybackClip(animator.previous, AssetConstants::c_InvalidAssetID);
			animator.fadeDuration = 0.0f;
			SetPlaybackClip(animator.current, animator.clip);
			animator.current.time = 0.0f;
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
		const float step = deltaTime * animator.playRate;

		// A clip still loading stays at its start.
		if (const AnimationClip* clip = m_AssetManager->GetAnimation(animator.current.clip))
		{
			animator.current.time = AnimationUtil::WrapTime(animator.current.time + step, clip->duration, animator.loop);
		}

		if (animator.fadeDuration > 0.0f)
		{
			if (const AnimationClip* clip = m_AssetManager->GetAnimation(animator.previous.clip))
			{
				animator.previous.time = AnimationUtil::WrapTime(animator.previous.time + step, clip->duration, animator.loop);
			}
			animator.fadeElapsed += deltaTime;
			if (animator.fadeElapsed >= animator.fadeDuration)
			{
				SetPlaybackClip(animator.previous, AssetConstants::c_InvalidAssetID);
				animator.fadeDuration = 0.0f;
			}
		}

		if (const AnimationClip* clip = m_AssetManager->GetAnimation(animator.layer.clip))
		{
			animator.layer.time = AnimationUtil::WrapTime(animator.layer.time + step, clip->duration, true);
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

	void AnimationSystem::PoseInstance(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator)
	{
		const MeshHeader* header = m_AssetManager->GetMeshHeader(skeletalMesh.mesh);
		const Skeleton* skeleton = header ? m_AssetManager->GetSkeleton(header->skeleton) : nullptr;
		if (!skeleton)
		{
			return;
		}

		const uint boneCount = skeleton->BoneCount();
		const AnimationClip* current = animator ? GetUsableClip(animator->current.clip, header->skeleton, *skeleton) : nullptr;
		if (current)
		{
			AnimationUtil::SampleClip(*current, *skeleton, animator->current.time, m_Pose);
		}
		else
		{
			memcpy(m_Pose, skeleton->bindPose.Data(), sizeof(BoneTransform) * boneCount);
		}

		if (animator && animator->fadeDuration > 0.0f)
		{
			if (const AnimationClip* previous = GetUsableClip(animator->previous.clip, header->skeleton, *skeleton))
			{
				// Blends from the old clip toward the new one as the fade goes on.
				AnimationUtil::SampleClip(*previous, *skeleton, animator->previous.time, m_BlendPose);
				const float weight = Math::Clamp01(animator->fadeElapsed / animator->fadeDuration);
				AnimationUtil::BlendPoses(m_BlendPose, m_Pose, weight, boneCount, m_Pose);
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
		AnimationUtil::ComputeSkinMatrices(*skeleton, m_ModelMatrices, m_SkinMatrices);
		const BoundingSphere bounds = AnimationUtil::ComputePoseBounds(m_ModelMatrices, boneCount, header->boneBoundsMargin);
		m_RendererAPI->UpdateSkeletalMeshPose(skeletalMesh.meshInstance, m_SkinMatrices, boneCount, bounds);
	}
}
