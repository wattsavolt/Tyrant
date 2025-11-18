#include "MaterialImporter.h"
#include "AssetSystem/MaterialAsset.h"
#include "ImageLoader.h"
#include "ImageCompressor.h"
#include "ImageUtil.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/TextureAsset.h"

namespace tyr
{
	MaterialImporter& MaterialImporter::Instance()
	{
		static MaterialImporter importer;
		return importer;
	}

	bool MaterialImporter::ImportAlbedoTexture(const char* outputFolderPath, const char* textureName, const char* albedoPath, bool isSRGB, AssetID& textureID, AssetID* refID) const
	{
		ImageInfo info;
		ImageLoader::LoadImageInfo(albedoPath, info);

		if (info.channelCount != 4)
		{
			TYR_LOG_ERROR("Invalid number of channels in texture %s.", albedoPath);
			return false;
		}

		Image2DCompressionDesc compDesc;

		switch (info.bitDepth)
		{
		case ImageBitDepth::EightBit:
		{
			compDesc.image = ImageLoader::LoadImage8U(albedoPath, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
			break;
		}
		case ImageBitDepth::SixteenBit:
		{
			compDesc.image = ImageLoader::LoadImage32F(albedoPath, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_16F;
			break;
		}
		case ImageBitDepth::ThirtyTwoBit:
		{
			compDesc.image = ImageLoader::LoadImage32F(albedoPath, 4);
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_32F;
			break;
		}
		default:
			return false;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s", outputFolderPath, textureName, c_TextureFileExtension);

		compDesc.assetID = AssetUtil::CreateAssetID();
		compDesc.width = info.width;
		compDesc.height = info.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = 0;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		AssetRegistry::Instance().AddAsset(compDesc.assetID, compDesc.outputFilePath, refID);

		textureID = compDesc.assetID;

		return true;
	}

	bool MaterialImporter::CreateAlbedo(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const
	{
		if (desc.albedoID != 0)
		{
			material.textures[MaterialConstants::c_PbrAlbedoIndex] = desc.albedoID;
			return true;
		}

		char textureName[PathConstants::c_MaxAssetNameTotalSize];
		snprintf(textureName, sizeof(textureName), "%s%s", desc.materialName, "_Albedo");

		AssetID textureID;

		if (!ImportAlbedoTexture(desc.outputFolderPath, textureName, desc.albedoPath, desc.isSRGB, textureID, &material.assetID))
		{
			return false;
		}

		material.textures[MaterialConstants::c_PbrAlbedoIndex] = textureID;

		return true;
	}

	bool MaterialImporter::CreateNormalHeight(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const
	{
		if (desc.normalHeightID != 0)
		{
			material.textures[MaterialConstants::c_PbrNormalHeightIndex] = desc.normalHeightID;
			return true;
		}

		ImageInfo normalInfo;
		ImageLoader::LoadImageInfo(desc.normalPath, normalInfo);

		ImageInfo heightInfo;
		ImageLoader::LoadImageInfo(desc.heightPath, heightInfo);

		if (normalInfo.width != heightInfo.width || normalInfo.height != heightInfo.height)
		{
			TYR_LOG_ERROR("The dimensions of the normal and height textures do not match for the PBR material %s.", desc.materialName);
			return false;
		}

		// Use the highest bit depth of any of the textures
		const ImageBitDepth bitDepth = static_cast<ImageBitDepth>(std::max(static_cast<uint8>(normalInfo.bitDepth),
			static_cast<uint8>(heightInfo.bitDepth)));

		Image2DCompressionDesc compDesc;

		const uint texelCount = normalInfo.width * normalInfo.height;
		switch (bitDepth)
		{
		case ImageBitDepth::EightBit:
		{
			uint8* normal = ImageLoader::LoadImage8U(desc.normalPath, 4);
			uint8* height = ImageLoader::LoadImage8U(desc.heightPath, 1);
			ImageUtil::CopyChannel<uint8>(height, normal, texelCount, 1, 4, 0, 3);
			ImageLoader::FreeImage(height);
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
			break;
		}
		case ImageBitDepth::SixteenBit:
		{
			float* normal = ImageLoader::LoadImage32F(desc.normalPath, 4);
			float* height = ImageLoader::LoadImage32F(desc.heightPath, 1);
			ImageUtil::CopyChannel<float>(height, normal, texelCount, 1, 4, 0, 3);
			ImageLoader::FreeImage(height);
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_16F;
			break;
		}
		case ImageBitDepth::ThirtyTwoBit:
		{
			float* normal = ImageLoader::LoadImage32F(desc.normalPath, 4);
			float* height = ImageLoader::LoadImage32F(desc.heightPath, 1);
			ImageUtil::CopyChannel<float>(height, normal, texelCount, 1, 4, 0, 3);
			ImageLoader::FreeImage(height);
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_32F;
			break;
		}
		default:
			return false;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, "_NormalHeight", c_TextureFileExtension);

		compDesc.assetID = AssetUtil::CreateAssetID();
		compDesc.width = normalInfo.width;
		compDesc.height = normalInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = 0;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = desc.isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		AssetRegistry::Instance().AddAsset(compDesc.assetID, compDesc.outputFilePath, &material.assetID);

		material.textures[MaterialConstants::c_PbrNormalHeightIndex] = compDesc.assetID;

		return true;
	}

	bool MaterialImporter::CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, MaterialAssetFile& material) const
	{
		if (desc.aoRoughnessMetallicID != 0)
		{
			material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = desc.aoRoughnessMetallicID;
			return true;
		}

		ImageInfo aoInfo;
		ImageLoader::LoadImageInfo(desc.ambientOcclusionPath, aoInfo);

		ImageInfo metallicInfo;
		ImageLoader::LoadImageInfo(desc.metallicPath, metallicInfo);

		ImageInfo roughnessInfo;

		uint reqMetallicChannelCount;
		if (desc.smoothnessInMetallic)
		{
			roughnessInfo = metallicInfo;
			reqMetallicChannelCount = 4;
		}
		else
		{
			ImageLoader::LoadImageInfo(desc.roughnessPath, roughnessInfo);
			reqMetallicChannelCount = 1;
		}

		if (aoInfo.width != roughnessInfo.width || aoInfo.height != roughnessInfo.height || aoInfo.width != metallicInfo.width || aoInfo.height != metallicInfo.height)
		{
			TYR_LOG_ERROR("The dimensions of the ambient occlusion, roughness and metallic textures do not match for the PBR material %s.", desc.materialName);
			return false;
		}

		if (metallicInfo.channelCount < reqMetallicChannelCount)
		{
			TYR_LOG_ERROR("The channel count of the metallic texture is less than required %ud for the PBR material %s.", reqMetallicChannelCount, desc.materialName);
			return false;
		}

		// Use the highest bit depth of any of the textures
		const ImageBitDepth bitDepth = static_cast<ImageBitDepth>(std::max(std::max(static_cast<uint8>(aoInfo.bitDepth),
			static_cast<uint8>(roughnessInfo.bitDepth)), static_cast<uint8>(metallicInfo.bitDepth)));

		Image2DCompressionDesc compDesc;

		const uint texelCount = aoInfo.width * aoInfo.height;
		if (bitDepth == ImageBitDepth::EightBit)
		{
			uint8* ao = ImageLoader::LoadImage8U(desc.ambientOcclusionPath, 4);
			uint8* metallic = ImageLoader::LoadImage8U(desc.metallicPath, reqMetallicChannelCount);
			ImageUtil::CopyChannel<uint8>(metallic, ao, texelCount, reqMetallicChannelCount, 4, 0, 2);

			// If smoothness is in metallic (Unity material), then there's no roughess texture
			if (desc.smoothnessInMetallic)
			{
				// Invert to convert smoothness to roughness
				ImageUtil::InvertChannel(metallic, texelCount, reqMetallicChannelCount, 3, desc.isSRGB);
				ImageUtil::CopyChannel<uint8>(metallic, ao, texelCount, reqMetallicChannelCount, 4, 3, 2);
			}
			else
			{
				uint8* roughness = ImageLoader::LoadImage8U(desc.roughnessPath, 1);
				ImageUtil::CopyChannel<uint8>(roughness, ao, texelCount, 1, 4, 0, 1);
				ImageLoader::FreeImage(roughness);
			}

			ImageLoader::FreeImage(metallic);
			compDesc.image = ao;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
		}
		else
		{
			float* ao = ImageLoader::LoadImage32F(desc.ambientOcclusionPath, 4);
			float* metallic = ImageLoader::LoadImage32F(desc.metallicPath, reqMetallicChannelCount);
			ImageUtil::CopyChannel<float>(metallic, ao, texelCount, reqMetallicChannelCount, 4, 0, 2);

			// If smoothness is in metallic (Unity material), then there's no roughess texture
			if (desc.smoothnessInMetallic)
			{
				// Invert to convert smoothness to roughness
				ImageUtil::InvertChannel(metallic, texelCount, reqMetallicChannelCount, 3, desc.isSRGB);
				ImageUtil::CopyChannel<float>(metallic, ao, texelCount, reqMetallicChannelCount, 4, 3, 2);
			}
			else
			{
				float* roughness = ImageLoader::LoadImage32F(desc.roughnessPath, 1);
				ImageUtil::CopyChannel<float>(roughness, ao, texelCount, 1, 4, 0, 1);
				ImageLoader::FreeImage(roughness);
			}

			ImageLoader::FreeImage(metallic);
			compDesc.image = ao;
			compDesc.inputFormat = bitDepth == ImageBitDepth::SixteenBit ? ImageCompressionInputFormat::RGBA_16F : ImageCompressionInputFormat::RGBA_32F;	
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, "_AORoughnessMetallic", c_TextureFileExtension);

		compDesc.assetID = AssetUtil::CreateAssetID();
		compDesc.width = aoInfo.width;
		compDesc.height = aoInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = 0;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = desc.isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		AssetRegistry::Instance().AddAsset(compDesc.assetID, compDesc.outputFilePath, &material.assetID);

		material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = compDesc.assetID;

		return true;
	}

	bool MaterialImporter::CreateMaterialAssetInfo(const char* outputFolderPath, const char* materialName, MaterialAssetFile& material) const
	{
		// Relative to the asset directory
		char materialPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(materialPath, sizeof(materialPath), "%s/%s%s", outputFolderPath, materialName, c_MaterialFileExtension);

		char absMaterialFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMaterialFolderPath, outputFolderPath);

		bool exists;
		const int refCount = AssetRegistry::Instance().GetAssetRefCount(materialPath);

		if (refCount > 0)
		{
			TYR_LOG_ERROR("Cannot overwrite material with references. Path: %s.", materialPath);
			return false;
		}
		else if (refCount == 0)
		{
			AssetRegistry::Instance().RemoveAssetIfExists(materialPath);
		}

		// Delete material directory if it exists and no references
		if (fs::exists(absMaterialFolderPath))
		{
			std::error_code ec;
			fs::remove_all(absMaterialFolderPath, ec);
			if (ec)
			{
				TYR_LOG_ERROR("Error deleting directory %s.", absMaterialFolderPath);
				return false;
			}
		}

		// Create material directory
		{
			std::error_code ec;
			if (!fs::create_directory(absMaterialFolderPath, ec))
			{
				TYR_LOG_ERROR("Error creating directory %s.", absMaterialFolderPath);
				return false;
			}
		}

		material.assetID = AssetUtil::CreateAssetID();
		AssetRegistry::Instance().AddAsset(material.assetID, materialPath);

		return true;
	}

	bool MaterialImporter::ImportPbrMaterial(const PbrMaterialImportDesc& desc) const
	{
		MaterialAssetFile material;
		material.type = MaterialType::PBR;

		if (!CreateMaterialAssetInfo(desc.outputFolderPath, desc.materialName, material))
		{
			return false;
		}

		if (!CreateAlbedo(desc, material) || !CreateNormalHeight(desc, material) || !CreateAORoughnessMetallic(desc, material))
		{
			return false;
		}

		return SerializeMaterial(desc, material);
	}

	bool MaterialImporter::SerializeMaterial(const PbrMaterialImportDesc& desc, const MaterialAssetFile& material) const
	{
		char materialPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(materialPath, sizeof(materialPath), "%s/%s%s", desc.outputFolderPath, desc.materialName, c_MaterialFileExtension);

		char absMaterialPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMaterialPath, materialPath);

		return true;
	}
}

	