#include "AnimationUtil.h"
#include <cmath>
#include <algorithm>

namespace tyr
{
	namespace
	{
		struct KeyPair
		{
			uint first;
			uint second;
			// How far from the first key to the second.
			float t;
		};

		KeyPair FindKeys(const AnimationClip& clip, float time)
		{
			KeyPair keys{ 0, 0, 0.0f };
			if (clip.frameCount < 2)
			{
				return keys;
			}

			const float frame = Math::Clamp(time * clip.sampleRate, 0.0f, static_cast<float>(clip.frameCount - 1));
			keys.first = static_cast<uint>(frame);
			if (keys.first >= clip.frameCount - 1)
			{
				keys.first = clip.frameCount - 1;
				keys.second = keys.first;
				return keys;
			}
			keys.second = keys.first + 1;
			keys.t = frame - static_cast<float>(keys.first);
			return keys;
		}

		Vector3 SampleVector(const Vector3* keys, uint count, const KeyPair& pair)
		{
			if (count == 1)
			{
				return keys[0];
			}
			return Vector3::Lerp(pair.t, keys[pair.first], keys[pair.second]);
		}
	}

	float AnimationUtil::WrapTime(float time, float duration, bool loop)
	{
		if (duration <= 0.0f)
		{
			return 0.0f;
		}
		if (!loop)
		{
			return Math::Clamp(time, 0.0f, duration);
		}
		const float wrapped = std::fmod(time, duration);
		return wrapped < 0.0f ? wrapped + duration : wrapped;
	}

	void AnimationUtil::SampleClip(const AnimationClip& clip, const Skeleton& skeleton, float time, BoneTransform* outPose)
	{
		const uint boneCount = skeleton.BoneCount();
		TYR_ASSERT(clip.tracks.Size() == boneCount);
		const KeyPair keys = FindKeys(clip, time);

		for (uint bone = 0; bone < boneCount; ++bone)
		{
			const AnimationTrack& track = clip.tracks[bone];
			BoneTransform& out = outPose[bone];
			out = skeleton.bindPose[bone];

			if (track.translationCount > 0)
			{
				out.translation = SampleVector(clip.translations.Data() + track.translationOffset, track.translationCount, keys);
			}
			if (track.rotationCount == 1)
			{
				out.rotation = clip.rotations[track.rotationOffset];
			}
			else if (track.rotationCount > 1)
			{
				const Quaternion* rotations = clip.rotations.Data() + track.rotationOffset;
				out.rotation = Quaternion::Lerp(keys.t, rotations[keys.first], rotations[keys.second]);
			}
			if (track.scaleCount > 0)
			{
				out.scale = SampleVector(clip.scales.Data() + track.scaleOffset, track.scaleCount, keys);
			}
		}
	}

	void AnimationUtil::BlendPoses(const BoneTransform* a, const BoneTransform* b, float weight, uint boneCount, BoneTransform* out)
	{
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			out[bone].translation = Vector3::Lerp(weight, a[bone].translation, b[bone].translation);
			out[bone].rotation = Quaternion::Lerp(weight, a[bone].rotation, b[bone].rotation);
			out[bone].scale = Vector3::Lerp(weight, a[bone].scale, b[bone].scale);
		}
	}

	void AnimationUtil::BlendPosesPerBone(const BoneTransform* a, const BoneTransform* b, const float* boneWeights, uint boneCount, BoneTransform* out)
	{
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			const float weight = boneWeights[bone];
			if (weight <= 0.0f)
			{
				out[bone] = a[bone];
				continue;
			}
			out[bone].translation = Vector3::Lerp(weight, a[bone].translation, b[bone].translation);
			out[bone].rotation = Quaternion::Lerp(weight, a[bone].rotation, b[bone].rotation);
			out[bone].scale = Vector3::Lerp(weight, a[bone].scale, b[bone].scale);
		}
	}

	void AnimationUtil::BuildBoneMask(const Skeleton& skeleton, uint16 maskRoot, float weight, float* outWeights)
	{
		const uint boneCount = skeleton.BoneCount();
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			// Parents come first, so a bone's parent is always decided before it.
			const uint16 parent = skeleton.parents[bone];
			const bool inMask = bone == maskRoot || (parent != SkeletonConstants::c_NoParent && outWeights[parent] > 0.0f);
			outWeights[bone] = inMask ? weight : 0.0f;
		}
	}

	void AnimationUtil::ComputeModelMatrices(const Skeleton& skeleton, const BoneTransform* localPose, Matrix4* outModel)
	{
		const uint boneCount = skeleton.BoneCount();
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			const BoneTransform& local = localPose[bone];
			const Matrix4 localMatrix = Matrix4::CreateTRS(local.translation, local.rotation, local.scale);
			const uint16 parent = skeleton.parents[bone];
			outModel[bone] = parent == SkeletonConstants::c_NoParent ? localMatrix : localMatrix * outModel[parent];
		}
	}

	void AnimationUtil::ComputeSkinMatrices(const Skeleton& skeleton, const Matrix4* modelMatrices, Matrix4* outSkin)
	{
		const uint boneCount = skeleton.BoneCount();
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			outSkin[bone] = skeleton.inverseBindMatrices[bone] * modelMatrices[bone];
		}
	}

	BoundingSphere AnimationUtil::ComputePoseBounds(const Matrix4* modelMatrices, uint boneCount, float margin)
	{
		if (boneCount == 0)
		{
			return BoundingSphere(Vector3::c_Zero, margin);
		}

		// Translation is in the last row.
		const Vector4& firstRow = modelMatrices[0][3];
		Vector3 boundsMin(firstRow.x, firstRow.y, firstRow.z);
		Vector3 boundsMax = boundsMin;
		for (uint bone = 1; bone < boneCount; ++bone)
		{
			const Vector4& row = modelMatrices[bone][3];
			boundsMin = Vector3(std::min(boundsMin.x, row.x), std::min(boundsMin.y, row.y), std::min(boundsMin.z, row.z));
			boundsMax = Vector3(std::max(boundsMax.x, row.x), std::max(boundsMax.y, row.y), std::max(boundsMax.z, row.z));
		}

		const Vector3 centre = (boundsMin + boundsMax) * 0.5f;
		return BoundingSphere(centre, (boundsMax - centre).Length() + margin);
	}
}
