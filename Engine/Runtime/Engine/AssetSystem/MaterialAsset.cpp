#include "MaterialAsset.h"

namespace tyr
{
	namespace
	{
		void SerializeParams(BufferedFileStream& stream, const MaterialParams& params)
		{
			Serialize<Vector4>(stream, params.albedoTint);
			Serialize<Vector2>(stream, params.uvScale);
			Serialize<Vector2>(stream, params.uvOffset);
			Serialize<float>(stream, params.roughnessMin);
			Serialize<float>(stream, params.roughnessMax);
			Serialize<float>(stream, params.metallicMin);
			Serialize<float>(stream, params.metallicMax);
			Serialize<float>(stream, params.normalStrength);
			Serialize<float>(stream, params.occlusionStrength);
			Serialize<float>(stream, params.heightScale);
			Serialize<float>(stream, params.alphaCutoff);
			Serialize<uint8>(stream, params.blendMode);
		}

		void DeserializeParams(BufferedFileStream& stream, MaterialParams& params)
		{
			Deserialize<Vector4>(stream, params.albedoTint);
			Deserialize<Vector2>(stream, params.uvScale);
			Deserialize<Vector2>(stream, params.uvOffset);
			Deserialize<float>(stream, params.roughnessMin);
			Deserialize<float>(stream, params.roughnessMax);
			Deserialize<float>(stream, params.metallicMin);
			Deserialize<float>(stream, params.metallicMax);
			Deserialize<float>(stream, params.normalStrength);
			Deserialize<float>(stream, params.occlusionStrength);
			Deserialize<float>(stream, params.heightScale);
			Deserialize<float>(stream, params.alphaCutoff);
			Deserialize<uint8>(stream, params.blendMode);
		}
	}

	template<>
	void Serialize(BufferedFileStream& stream, const MaterialAssetFile& file)
	{
		Serialize<uint>(stream, MaterialAssetFile::c_FileVersion);
		Serialize(stream, file.textures);
		Serialize<uint8>(stream, static_cast<uint8>(file.type));
		SerializeParams(stream, file.params);
	}

	template<>
	void Deserialize(BufferedFileStream& stream, MaterialAssetFile& file)
	{
		file = MaterialAssetFile();
		uint version;
		Deserialize<uint>(stream, version);

		// A file from before versioning starts with its texture count and has no parameters.
		const bool unversioned = version <= MaterialConstants::c_MaxTextures;
		TYR_ASSERT(unversioned || version == MaterialAssetFile::c_FileVersion);
		if (unversioned)
		{
			for (uint i = 0; i < version; ++i)
			{
				Deserialize<AssetID>(stream, file.textures.ExpandOne());
			}
		}
		else
		{
			Deserialize(stream, file.textures);
		}

		uint8 type;
		Deserialize<uint8>(stream, type);
		file.type = static_cast<MaterialType>(type);
		if (!unversioned)
		{
			DeserializeParams(stream, file.params);
		}
	}
}
