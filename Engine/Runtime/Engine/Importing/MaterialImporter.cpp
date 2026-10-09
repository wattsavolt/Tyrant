#include "MaterialImporter.h"
#include "AssetSystem/MaterialAsset.h"
#include "ImageLoader.h"
#include "ImageCompressor.h"
#include "ImageUtil.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/TextureAsset.h"
#include "AssetSystem/AssetConstants.h"
#include "Rendering/RenderConstants.h"
#include "Utility/PathUtil.h"
#include <cstring>

namespace tyr
{
	namespace
	{
		// Creates the folder, relative to the assets folder, if it doesn't exist yet.
		bool CreateAssetFolder(const char* relativeFolderPath)
		{
			char absFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absFolderPath, relativeFolderPath);

			if (!PathUtil::CreateDirectories(absFolderPath))
			{
				TYR_LOG_ERROR("Error creating directory %s.", absFolderPath);
				return false;
			}
			return true;
		}
	}

	MaterialImporter& MaterialImporter::Instance()
	{
		static MaterialImporter importer;
		return importer;
	}

	MaterialImporter::MaterialImporter()
	{
		snprintf(m_DefaultMaterialPath, sizeof(m_DefaultMaterialPath), "%s/%s%s", AssetConstants::c_DefaultMaterialFolderName, AssetConstants::c_DefaultMaterialName, AssetConstants::c_MaterialFileExtension);
	}

	AssetID MaterialImporter::ResolveDefaultTextureAssetID(const char* suffix, AssetID& cachedID)
	{
		if (!AssetUtil::IsValidAssetID(cachedID))
		{
			char path[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(path, sizeof(path), "%s/%s%s%s", AssetConstants::c_DefaultMaterialFolderName, AssetConstants::c_DefaultMaterialName, suffix, AssetConstants::c_TextureFileExtension);
			cachedID = AssetRegistry::Instance().GetAssetID(path);
		}
		return cachedID;
	}

	AssetID MaterialImporter::GetDefaultMaterialAssetID()
	{
		if (!AssetUtil::IsValidAssetID(m_DefaultMaterialAssetID))
		{
			m_DefaultMaterialAssetID = AssetRegistry::Instance().GetAssetID(m_DefaultMaterialPath);
		}
		return m_DefaultMaterialAssetID;
	}

	AssetID MaterialImporter::GetDefaultAlbedoAssetID()
	{
		return ResolveDefaultTextureAssetID(AssetConstants::c_AlbedoTextureSuffix, m_DefaultAlbedoAssetID);
	}

	AssetID MaterialImporter::GetDefaultNormalHeightAssetID()
	{
		return ResolveDefaultTextureAssetID(AssetConstants::c_NormalHeightTextureSuffix, m_DefaultNormalHeightAssetID);
	}

	AssetID MaterialImporter::GetDefaultAORoughnessMetallicAssetID()
	{
		return ResolveDefaultTextureAssetID(AssetConstants::c_AORoughnessMetallicTextureSuffix, m_DefaultAORoughnessMetallicAssetID);
	}

	bool LoadImageInfo(const TextureSource& source, ImageInfo& outInfo)
	{
		if (source.path) 
		{
			ImageLoader::LoadImageInfo(source.path, outInfo);
			return true;
		}
		else if (source.data) 
		{
			ImageLoader::LoadImageInfoFromMem(source.data, source.dataSize, outInfo);
			return true;
		}
		return false;
	}

	uint8* LoadImage8U(const TextureSource& source, int channelCount)
	{
		if (source.path)
		{
			return ImageLoader::LoadImage8U(source.path, channelCount);
		}
		else if (source.data)
		{
			return ImageLoader::LoadImage8UFromMem(source.data, source.dataSize, channelCount);
		}
		return nullptr;
	}

	uint16* LoadImage16U(const TextureSource& source, int channelCount)
	{
		if (source.path)
		{
			return ImageLoader::LoadImage16U(source.path, channelCount);
		}
		else if (source.data)
		{
			return ImageLoader::LoadImage16UFromMem(source.data, source.dataSize, channelCount);
		}
		return nullptr;
	}

	float* LoadImage32F(const TextureSource& source, int channelCount)
	{
		if (source.path)
		{
			return ImageLoader::LoadImage32F(source.path, channelCount);
		}
		else if (source.data)
		{
			return ImageLoader::LoadImage32FFromMem(source.data, source.dataSize, channelCount);
		}
		return nullptr;
	}

	// True if both sources point at the same underlying image, so callers can avoid decoding it twice.
	bool IsSameTextureSource(const TextureSource& a, const TextureSource& b)
	{
		if (a.path && b.path)
		{
			return std::strcmp(a.path, b.path) == 0;
		}
		if (a.data && b.data)
		{
			return a.data == b.data && a.dataSize == b.dataSize;
		}
		return false;
	}

	bool MaterialImporter::ImportTexture(const TextureSource& source, const char* outputFolderPath, const char* textureName, bool isSRGB, AssetID& textureID)
	{
		if (!CreateAssetFolder(outputFolderPath))
		{
			return false;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s", outputFolderPath, textureName, AssetConstants::c_TextureFileExtension);

		ImageInfo info;
		LoadImageInfo(source, info);

		if (info.channelCount != 3 && info.channelCount != 4)
		{
			TYR_LOG_ERROR("Invalid number of channels in source texture when trying to create %s.", textureName);
			return false;
		}

		Image2DCompressionDesc compDesc;
		// Force 4 channels even when there is no alpha channel as nvtt only support RGBA as input
		switch (info.bitDepth)
		{
		case ImageBitDepth::EightBit:
		{
			compDesc.image = LoadImage8U(source, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
			break;
		}
		case ImageBitDepth::SixteenBit:
		{
			compDesc.image = LoadImage32F(source, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_16F;
			break;
		}
		case ImageBitDepth::ThirtyTwoBit:
		{
			compDesc.image = LoadImage32F(source, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_32F;
			break;
		}
		default:
			return false;
		}

		compDesc.width = info.width;
		compDesc.height = info.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		// Capped to this texture's own size, not the engine-wide max mip count.
		compDesc.mipCount = TextureUtil::CalculateMaxMips(compDesc.width, compDesc.height);
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		textureID = AssetRegistry::Instance().AddOrUpdateAsset(outputPath);
		return true;
	}

	bool MaterialImporter::CreateAlbedo(const PbrMaterialImportDesc& desc, MaterialAssetFile& material)
	{
		if (desc.albedoID != 0)
		{
			material.textures[MaterialConstants::c_PbrAlbedoIndex] = desc.albedoID;
			return true;
		}

		if (!desc.albedoSource.IsPresent())
		{
			material.textures[MaterialConstants::c_PbrAlbedoIndex] = GetDefaultAlbedoAssetID();
			return true;
		}

		char textureName[PathConstants::c_MaxAssetNameTotalSize];
		snprintf(textureName, sizeof(textureName), "%s%s", desc.materialName, AssetConstants::c_AlbedoTextureSuffix);

		AssetID textureID;

		if (!ImportTexture(desc.albedoSource, desc.outputFolderPath, textureName, desc.isSRGB, textureID))
		{
			return false;
		}

		material.textures[MaterialConstants::c_PbrAlbedoIndex] = textureID;

		return true;
	}

	bool MaterialImporter::CreateNormalHeight(const PbrMaterialImportDesc& desc, MaterialAssetFile& material)
	{
		if (desc.normalHeightID != 0)
		{
			material.textures[MaterialConstants::c_PbrNormalHeightIndex] = desc.normalHeightID;
			return true;
		}

		if (!desc.normalSource.IsPresent())
		{
			material.textures[MaterialConstants::c_PbrNormalHeightIndex] = GetDefaultNormalHeightAssetID();
			return true;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, AssetConstants::c_NormalHeightTextureSuffix, AssetConstants::c_TextureFileExtension);

		ImageInfo normalInfo;
		LoadImageInfo(desc.normalSource, normalInfo);

		ImageInfo heightInfo;
		const bool heightPresent = desc.heightSource.IsPresent();
		if (heightPresent)
		{
			LoadImageInfo(desc.heightSource, heightInfo);
		}
		
		if (heightPresent)
		{
			if (normalInfo.width != heightInfo.width || normalInfo.height != heightInfo.height)
			{
				TYR_LOG_ERROR("The dimensions of the normal and height textures do not match for the PBR material %s.", desc.materialName);
				return false;
			}
		}

		// Use the highest bit depth of the textures. heightInfo is only set when there's a height map.
		const ImageBitDepth bitDepth = heightPresent
			? static_cast<ImageBitDepth>(std::max(static_cast<uint8>(normalInfo.bitDepth), static_cast<uint8>(heightInfo.bitDepth)))
			: normalInfo.bitDepth;

		Image2DCompressionDesc compDesc;

		const uint texelCount = normalInfo.width * normalInfo.height;
		switch (bitDepth)
		{
		case ImageBitDepth::EightBit:
		{
			uint8* normal = LoadImage8U(desc.normalSource, 4);
			if (heightPresent)
			{
				uint8* height = LoadImage8U(desc.heightSource, 1);
				ImageUtil::CopyChannel<uint8>(height, normal, texelCount, 1, 4, 0, 3);
				ImageLoader::FreeImage(height);
			}
			else
			{
				// No height map, so use a neutral middle height.
				ImageUtil::FillChannel<uint8>(normal, texelCount, 4, 3, 128);
			}
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
			break;
		}
		case ImageBitDepth::SixteenBit:
		{
			float* normal = LoadImage32F(desc.normalSource, 4);
			if (heightPresent)
			{
				float* height = LoadImage32F(desc.heightSource, 1);
				ImageUtil::CopyChannel<float>(height, normal, texelCount, 1, 4, 0, 3);
				ImageLoader::FreeImage(height);
			}
			else
			{
				ImageUtil::FillChannel<float>(normal, texelCount, 4, 3, 0.5f);
			}
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_16F;
			break;
		}
		case ImageBitDepth::ThirtyTwoBit:
		{
			float* normal = LoadImage32F(desc.normalSource, 4);
			if (heightPresent)
			{
				float* height = LoadImage32F(desc.heightSource, 1);
				ImageUtil::CopyChannel<float>(height, normal, texelCount, 1, 4, 0, 3);
				ImageLoader::FreeImage(height);
			}
			else
			{
				ImageUtil::FillChannel<float>(normal, texelCount, 4, 3, 0.5f);
			}
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_32F;
			break;
		}
		default:
			return false;
		}

		compDesc.width = normalInfo.width;
		compDesc.height = normalInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		// Capped to this texture's own size, not the engine-wide max mip count.
		compDesc.mipCount = TextureUtil::CalculateMaxMips(compDesc.width, compDesc.height);
		compDesc.outputFilePath = outputPath;
		// Never sRGB - this is a normal + height, not colour data.
		compDesc.isSRGB = false;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		material.textures[MaterialConstants::c_PbrNormalHeightIndex] = AssetRegistry::Instance().AddOrUpdateAsset(outputPath);
		return true;
	}

	bool MaterialImporter::CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, MaterialAssetFile& material)
	{
		if (desc.aoRoughnessMetallicID != 0)
		{
			material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = desc.aoRoughnessMetallicID;
			return true;
		}

		if (!desc.roughnessMetallicSource.IsPresent())
		{
			material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = GetDefaultAORoughnessMetallicAssetID();
			return true;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, AssetConstants::c_AORoughnessMetallicTextureSuffix, AssetConstants::c_TextureFileExtension);

		// Occlusion is often already packed into the roughness/metallic texture's red channel.
		const bool sameSource = IsSameTextureSource(desc.occlusionSource, desc.roughnessMetallicSource);
		const bool occlusionPresent = desc.occlusionSource.IsPresent();

		ImageInfo roughnessMetallicInfo;
		LoadImageInfo(desc.roughnessMetallicSource, roughnessMetallicInfo);

		// Only set when there's occlusion data.
		ImageInfo occlusionInfo;
		if (sameSource)
		{
			occlusionInfo = roughnessMetallicInfo;
		}
		else if (occlusionPresent)
		{
			LoadImageInfo(desc.occlusionSource, occlusionInfo);

			if (occlusionInfo.width != roughnessMetallicInfo.width || occlusionInfo.height != roughnessMetallicInfo.height)
			{
				TYR_LOG_ERROR("The dimensions of the occlusion and roughnessMetallic texture do not match for the PBR material %s.", desc.materialName);
				return false;
			}
		}

		const uint minReqRoughnessMetallicChannelCount = 3;
		if (roughnessMetallicInfo.channelCount < minReqRoughnessMetallicChannelCount)
		{
			TYR_LOG_ERROR("The channel count of the metallic texture is less than required %ud for the PBR material %s.", minReqRoughnessMetallicChannelCount, desc.materialName);
			return false;
		}

		const bool hasOcclusionInfo = sameSource || occlusionPresent;
		const ImageBitDepth bitDepth = hasOcclusionInfo
			? static_cast<ImageBitDepth>(std::max(static_cast<uint8>(occlusionInfo.bitDepth), static_cast<uint8>(roughnessMetallicInfo.bitDepth)))
			: roughnessMetallicInfo.bitDepth;

		Image2DCompressionDesc compDesc;

		const uint texelCount = roughnessMetallicInfo.width * roughnessMetallicInfo.height;
		if (bitDepth == ImageBitDepth::EightBit)
		{
			uint8* roughnessMetallic = LoadImage8U(desc.roughnessMetallicSource, 4);
			if (!sameSource)
			{
				if (occlusionPresent)
				{
					uint8* occlusion = LoadImage8U(desc.occlusionSource, 1);
					// Write occlusion to channel 0 of combined texture
					ImageUtil::CopyChannel<uint8>(occlusion, roughnessMetallic, texelCount, 1, 4, 0, 0);
					ImageLoader::FreeImage(occlusion);
				}
				else
				{
					// No occlusion map, so treat it as fully lit.
					ImageUtil::FillChannel<uint8>(roughnessMetallic, texelCount, 4, 0, 255);
				}
			}
			compDesc.image = roughnessMetallic;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
		}
		else
		{
			float* roughnessMetallic = LoadImage32F(desc.roughnessMetallicSource, 4);
			if (!sameSource)
			{
				if (occlusionPresent)
				{
					float* occlusion = LoadImage32F(desc.occlusionSource, 1);
					// Write occlusion to channel 0 of combined texture
					ImageUtil::CopyChannel<float>(occlusion, roughnessMetallic, texelCount, 1, 4, 0, 0);
					ImageLoader::FreeImage(occlusion);
				}
				else
				{
					ImageUtil::FillChannel<float>(roughnessMetallic, texelCount, 4, 0, 1.0f);
				}
			}
			compDesc.image = roughnessMetallic;
			compDesc.inputFormat = bitDepth == ImageBitDepth::SixteenBit ? ImageCompressionInputFormat::RGBA_16F : ImageCompressionInputFormat::RGBA_32F;
		}

		compDesc.width = roughnessMetallicInfo.width;
		compDesc.height = roughnessMetallicInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		// Capped to this texture's own size, not the engine-wide max mip count.
		compDesc.mipCount = TextureUtil::CalculateMaxMips(compDesc.width, compDesc.height);
		compDesc.outputFilePath = outputPath;
		// Never sRGB - AO/roughness/metallic are scalar parameters, not colour.
		compDesc.isSRGB = false;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = AssetRegistry::Instance().AddOrUpdateAsset(outputPath);
		return true;
	}

	bool MaterialImporter::ImportPbrMaterial(const PbrMaterialImportDesc& desc, AssetID& materialID)
	{
		if (!CreateAssetFolder(desc.outputFolderPath))
		{
			return false;
		}

		MaterialAssetFile material;
		material.type = MaterialType::PBR;
		// Sized up front because the textures below are written by index.
		material.textures.Resize(MaterialConstants::c_PbrAoRoughnessMetallicIndex + 1);

		if (!CreateAlbedo(desc, material) || !CreateNormalHeight(desc, material) || !CreateAORoughnessMetallic(desc, material))
		{
			return false;
		}

		char materialPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(materialPath, sizeof(materialPath), "%s/%s%s", desc.outputFolderPath, desc.materialName, AssetConstants::c_MaterialFileExtension);

		AssetUtil::SaveAsset<MaterialAssetFile>(materialPath, material);
		materialID = AssetRegistry::Instance().AddOrUpdateAsset(materialPath, material.textures.Data(), material.textures.Size());
		return true;
	}
}

	