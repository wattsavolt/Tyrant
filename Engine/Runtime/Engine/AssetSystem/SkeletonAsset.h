#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "IO/BufferedFileStream.h"
#include "Shaders/ShaderTypes.h"

namespace tyr
{
	struct SkeletonConstants
	{
		// Skinned vertices store bone indices as uint8, so this can't go higher.
		static constexpr uint c_MaxBones = 256;
		static constexpr uint16 c_NoParent = 0xFFFF;
		static constexpr uint c_MaxBoneName = 63;
	};
	static_assert(SkeletonConstants::c_MaxBones == TYR_MAX_BONES, "Keep in sync with ShaderTypes.h's TYR_MAX_BONES.");

	using BoneName = LocalString<SkeletonConstants::c_MaxBoneName>;

	// A bone's transform relative to its parent.
	struct BoneTransform
	{
		Vector3 translation;
		Quaternion rotation;
		Vector3 scale;
	};

	// Bones are stored parents first, so a single forward pass resolves model space.
	struct Skeleton
	{
		Array<uint16> parents;
		Array<BoneName> names;
		Array<BoneTransform> bindPose;
		// Model space to bone space at bind time.
		Array<Matrix4> inverseBindMatrices;

		uint BoneCount() const { return parents.Size(); }

		// Returns c_NoParent if no bone has this name.
		uint16 FindBone(const char* name) const;

		void Reset()
		{
			parents.Clear();
			names.Clear();
			bindPose.Clear();
			inverseBindMatrices.Clear();
		}
	};

	template<> void Serialize(BufferedFileStream& stream, const Skeleton& skeleton);
	template<> void Deserialize(BufferedFileStream& stream, Skeleton& skeleton);
}
