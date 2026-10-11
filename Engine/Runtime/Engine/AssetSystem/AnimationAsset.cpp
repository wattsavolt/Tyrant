#include "AnimationAsset.h"
#include "AssetSerializationUtil.h"

namespace tyr
{
	template<>
	void Serialize(BufferedFileStream& stream, const AnimationClip& clip)
	{
		stream.Write(&clip.skeleton, sizeof(clip.skeleton));
		stream.Write(&clip.duration, sizeof(clip.duration));
		stream.Write(&clip.sampleRate, sizeof(clip.sampleRate));
		stream.Write(&clip.frameCount, sizeof(clip.frameCount));
		AssetSerializationUtil::WriteArray(stream, clip.tracks);
		AssetSerializationUtil::WriteArray(stream, clip.translations);
		AssetSerializationUtil::WriteArray(stream, clip.rotations);
		AssetSerializationUtil::WriteArray(stream, clip.scales);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, AnimationClip& clip)
	{
		stream.Read(&clip.skeleton, sizeof(clip.skeleton));
		stream.Read(&clip.duration, sizeof(clip.duration));
		stream.Read(&clip.sampleRate, sizeof(clip.sampleRate));
		stream.Read(&clip.frameCount, sizeof(clip.frameCount));
		AssetSerializationUtil::ReadArray(stream, clip.tracks);
		AssetSerializationUtil::ReadArray(stream, clip.translations);
		AssetSerializationUtil::ReadArray(stream, clip.rotations);
		AssetSerializationUtil::ReadArray(stream, clip.scales);
	}
}
