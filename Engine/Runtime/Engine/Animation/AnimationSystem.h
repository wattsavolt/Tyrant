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
	struct LimbCollisionComponent;

	// How a clip is started by AnimationSystem::Play.
	struct AnimationPlayDesc
	{
		// Seconds it takes to fade in over what's playing. Zero switches straight to it.
		float fadeDuration = 0.25f;
		float playRate = 1.0f;
		bool loop = true;
		bool inPlace = false;
		// Starts it again even if it's the clip already playing, such as for another swing.
		bool restart = false;
		// Starts it as far through as the clip it replaces, so steps carry on from walking into running.
		bool matchPhase = false;
	};

	// Plays each AnimatorComponent's clips and poses the skeletal mesh on the same entity.
	class TYR_ENGINE_API AnimationSystem final : INonCopyable
	{
	public:
		void Initialize(AssetManager& assetManager, RendererAPI& rendererAPI);

		// Clips only move on in a simulated world. One being edited shows each clip's first frame.
		void Update(World& world, float deltaTime);

		// Fades the clip in over whatever the animator is playing.
		void Play(AnimatorComponent& animator, AssetID clip, const AnimationPlayDesc& desc);

		// Cross-fades to the clip over fadeDuration seconds, looping it if the animator loops.
		void Play(AnimatorComponent& animator, AssetID clip, float fadeDuration);

		// Changes how fast the newest clip plays.
		void SetPlayRate(AnimatorComponent& animator, float playRate);

		// Seconds until the newest clip ends at its play rate. Very large for a looping clip or one
		// still loading.
		float GetTimeLeft(const AnimatorComponent& animator) const;

		// How fast the newest clip's root bone travels across the ground, in metres per second at a
		// play rate of 1. Zero while it's loading or if it stays in place.
		float GetGroundSpeed(const AnimatorComponent& animator) const;

		// Lets go of the clips the animator holds, before it's removed.
		void Release(AnimatorComponent& animator);

		// How many bones the mesh's skeleton has. Zero until it's loaded.
		uint GetBoneCount(const SkeletalMeshComponent& skeletalMesh) const;

		// Where the bone is as the animator poses it now, relative to the mesh. False until it's loaded.
		bool GetBonePosition(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, uint bone, Vector3& outPosition);

		// Every bone's transform as the animator poses it now, relative to the mesh, kept until the
		// next pose is asked for, and the skeleton they belong to. Returns the bone count, 0 until loaded.
		uint GetModelPose(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, const Matrix4*& outMatrices, const Skeleton*& outSkeleton);

	private:
		// Picks up clips changed on the component itself, fading to a new base clip while simulating.
		void SyncClips(AnimatorComponent& animator, bool simulate);
		void SetPlaybackClip(AnimationPlayback& playback, AssetID clip);
		void Advance(AnimatorComponent& animator, float deltaTime);
		// Null if the clip isn't loaded or doesn't fit the skeleton.
		const AnimationClip* GetUsableClip(AssetID clip, AssetID skeletonID, const Skeleton& skeleton) const;
		void SamplePlayback(const AnimationClip& clip, const Skeleton& skeleton, const AnimationPlayback& playback, BoneTransform* outPose) const;
		void PoseInstance(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator, const LimbCollisionComponent* limbs);
		// Swings each arm out of whatever it would go into, by the turn LimbCollision worked out.
		void TurnArms(const Skeleton& skeleton, const LimbCollisionComponent& limbs, uint boneCount);
		// Fills m_ModelMatrices with the animator's current pose. Returns the bone count, or 0 until loaded.
		uint ComputePose(const SkeletalMeshComponent& skeletalMesh, const AnimatorComponent* animator);

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
