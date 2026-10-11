#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/SkeletonAsset.h"
#include "AssetSystem/AnimationAsset.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	class AssetManager;
	class RendererAPI;
	struct World;
	struct MeshHeader;
	struct AnimatorComponent;
	struct AnimationPlayback;
	struct SkeletalMeshComponent;

	// Plays each AnimatorComponent's clips and poses the skeletal mesh on the same entity.
	class TYR_ENGINE_API AnimationSystem final : INonCopyable
	{
	public:
		void Initialize(AssetManager& assetManager, RendererAPI& rendererAPI);

		// Clips only move on in a simulated world. One being edited shows each clip's first frame.
		void Update(World& world, float deltaTime);

		// Cross-fades the animator's base clip to the new one over fadeDuration seconds.
		void Play(AnimatorComponent& animator, AssetID clip, float fadeDuration);

		// Lets go of the clips the animator holds, before it's removed.
		void Release(AnimatorComponent& animator);

	private:
		// Picks up clips changed on the component itself, fading to a new base clip while simulating.
		void SyncClips(AnimatorComponent& animator, bool simulate);
		void SetPlaybackClip(AnimationPlayback& playback, AssetID clip);
		void Advance(AnimatorComponent& animator, float deltaTime);
		// Null if the clip isn't loaded or doesn't fit the skeleton.
		const AnimationClip* GetUsableClip(AssetID clip, AssetID skeletonID, const Skeleton& skeleton) const;
		void PoseInstance(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator);

		AssetManager* m_AssetManager = nullptr;
		RendererAPI* m_RendererAPI = nullptr;

		// Scratch for posing one instance at a time.
		BoneTransform m_Pose[SkeletonConstants::c_MaxBones];
		BoneTransform m_BlendPose[SkeletonConstants::c_MaxBones];
		float m_BoneWeights[SkeletonConstants::c_MaxBones];
		Matrix4 m_ModelMatrices[SkeletonConstants::c_MaxBones];
		Matrix4 m_SkinMatrices[SkeletonConstants::c_MaxBones];
	};
}
