#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/SkeletonAsset.h"
#include "AssetSystem/AnimationAsset.h"
#include "Geometry/BoundingSphere.h"

namespace tyr
{
	// Pose functions work on arrays of one entry per skeleton bone.
	class TYR_ENGINE_API AnimationUtil final
	{
	public:
		// Wraps a looping clip's time back into range, or clamps one that doesn't loop.
		static float WrapTime(float time, float duration, bool loop);

		// Samples each bone at time, interpolating between the two keys either side of it.
		// Bones without keys keep their bind pose.
		static void SampleClip(const AnimationClip& clip, const Skeleton& skeleton, float time, BoneTransform* outPose);

		// Moves pose a toward b by weight, 0 keeping a and 1 giving b. out may be a or b.
		static void BlendPoses(const BoneTransform* a, const BoneTransform* b, float weight, uint boneCount, BoneTransform* out);

		// Like BlendPoses with a weight per bone, for playing a clip over part of the body.
		static void BlendPosesPerBone(const BoneTransform* a, const BoneTransform* b, const float* boneWeights, uint boneCount, BoneTransform* out);

		// Gives maskRoot and every bone under it the weight, and the other bones 0.
		static void BuildBoneMask(const Skeleton& skeleton, uint16 maskRoot, float weight, float* outWeights);

		// Each bone's transform relative to the skeleton's root space.
		static void ComputeModelMatrices(const Skeleton& skeleton, const BoneTransform* localPose, Matrix4* outModel);

		// What each bone does to the vertices bound to it, for the GPU to skin with.
		static void ComputeSkinMatrices(const Skeleton& skeleton, const Matrix4* modelMatrices, Matrix4* outSkin);

		// Bounds the posed mesh by its bones, grown by how far vertices sit from them.
		static BoundingSphere ComputePoseBounds(const Matrix4* modelMatrices, uint boneCount, float margin);
	};
}
