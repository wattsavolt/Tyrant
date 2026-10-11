#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetID.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "IO/BufferedFileStream.h"

namespace tyr
{
	struct AnimationConstants
	{
		// Keys per second clips are resampled to on import.
		static constexpr float c_SampleRate = 30.0f;
	};

	// Where one bone's keys live in its clip. A channel has 0 keys (the bone keeps its bind
	// pose), 1 key (it never changes) or one key per frame of the clip.
	struct AnimationTrack
	{
		uint translationOffset = 0;
		uint translationCount = 0;
		uint rotationOffset = 0;
		uint rotationCount = 0;
		uint scaleOffset = 0;
		uint scaleCount = 0;
	};

	// Keys are evenly spaced, so sampling finds its two keys without searching.
	struct AnimationClip
	{
		AssetID skeleton;
		float duration = 0.0f;
		float sampleRate = AnimationConstants::c_SampleRate;
		// Keys in an animated channel. The last one is at the clip's duration.
		uint frameCount = 0;
		// One per bone of the skeleton, in the skeleton's bone order.
		Array<AnimationTrack> tracks;
		Array<Vector3> translations;
		Array<Quaternion> rotations;
		Array<Vector3> scales;

		void Reset()
		{
			skeleton = {};
			duration = 0.0f;
			sampleRate = AnimationConstants::c_SampleRate;
			frameCount = 0;
			tracks.Clear();
			translations.Clear();
			rotations.Clear();
			scales.Clear();
		}
	};

	template<> void Serialize(BufferedFileStream& stream, const AnimationClip& clip);
	template<> void Deserialize(BufferedFileStream& stream, AnimationClip& clip);
}
