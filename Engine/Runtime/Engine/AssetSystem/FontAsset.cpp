#include "FontAsset.h"
#include "AssetSerializationUtil.h"

namespace tyr
{
	template<>
	void Serialize(BufferedFileStream& stream, const FontFile& font)
	{
		Serialize<uint>(stream, FontFile::c_FileVersion);
		AssetSerializationUtil::WriteArray(stream, font.data);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, FontFile& font)
	{
		uint fileVersion = 0;
		Deserialize<uint>(stream, fileVersion);
		if (fileVersion != FontFile::c_FileVersion)
		{
			TYR_LOG_ERROR("A font was saved in an older format. Import it again.");
			return;
		}
		AssetSerializationUtil::ReadArray(stream, font.data);
	}
}
