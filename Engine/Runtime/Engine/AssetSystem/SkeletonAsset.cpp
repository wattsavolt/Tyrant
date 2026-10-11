#include "SkeletonAsset.h"
#include "AssetSerializationUtil.h"

namespace tyr
{
	uint16 Skeleton::FindBone(const char* name) const
	{
		for (uint i = 0; i < names.Size(); ++i)
		{
			if (strcmp(names[i].CStr(), name) == 0)
			{
				return static_cast<uint16>(i);
			}
		}
		return SkeletonConstants::c_NoParent;
	}

	template<>
	void Serialize(BufferedFileStream& stream, const Skeleton& skeleton)
	{
		AssetSerializationUtil::WriteArray(stream, skeleton.parents);
		AssetSerializationUtil::WriteArray(stream, skeleton.bindPose);
		AssetSerializationUtil::WriteArray(stream, skeleton.inverseBindMatrices);
		for (const BoneName& name : skeleton.names)
		{
			AssetSerializationUtil::WriteString(stream, name.CStr());
		}
	}

	template<>
	void Deserialize(BufferedFileStream& stream, Skeleton& skeleton)
	{
		AssetSerializationUtil::ReadArray(stream, skeleton.parents);
		AssetSerializationUtil::ReadArray(stream, skeleton.bindPose);
		AssetSerializationUtil::ReadArray(stream, skeleton.inverseBindMatrices);
		skeleton.names.Resize(skeleton.parents.Size());
		for (BoneName& name : skeleton.names)
		{
			AssetSerializationUtil::ReadString(stream, name);
		}
	}
}
