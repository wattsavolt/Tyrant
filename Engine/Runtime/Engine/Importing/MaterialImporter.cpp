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
#include <cstring>

namespace tyr
{
	MaterialImporter& MaterialImporter::Instance()
	{
		static MaterialImporter importer;
		return importer;
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
			ImageLoader::LoadImage8U(source.path, channelCount);
		}
		else if (source.data)
		{
			ImageLoader::LoadImage8UFromMem(source.data, source.dataSize, channelCount);
		}
		return nullptr;
	}

	uint16* LoadImage16U(const TextureSource& source, int channelCount)
	{
		if (source.path)
		{
			ImageLoader::LoadImage16U(source.path, channelCount);
		}
		else if (source.data)
		{
			ImageLoader::LoadImage16UFromMem(source.data, source.dataSize, channelCount);
		}
		return nullptr;
	}

	float* LoadImage32F(const TextureSource& source, int channelCount)
	{
		if (source.path)
		{
			ImageLoader::LoadImage32F(source.path, channelCount);
		}
		else if (source.data)
		{
			ImageLoader::LoadImage32FFromMem(source.data, source.dataSize, channelCount);
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

	bool MaterialImporter::ImportAlbedoTexture(const TextureSource& source, const char* outputFolderPath, const char* textureName, bool isSRGB, AssetID& textureID) const
	{
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

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s", outputFolderPath, textureName, AssetConstants::c_TextureFileExtension);

		compDesc.width = info.width;
		compDesc.height = info.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = RenderConstants::c_MaxMips;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		textureID = AssetUtil::CreateAssetID();

		AssetRegistry::Instance().AddAsset(textureID, compDesc.outputFilePath);

		return true;
	}

	bool MaterialImporter::CreateAlbedo(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const
	{
		if (desc.albedoID != 0)
		{
			material.textures[MaterialConstants::c_PbrAlbedoIndex] = desc.albedoID;
			return true;
		}

		char textureName[PathConstants::c_MaxAssetNameTotalSize];
		snprintf(textureName, sizeof(textureName), "%s%s", desc.materialName, "_Albedo");

		AssetID textureID;

		if (!ImportAlbedoTexture(desc.albedoSource, desc.outputFolderPath, textureName, desc.isSRGB, textureID))
		{
			return false;
		}

		material.textures[MaterialConstants::c_PbrAlbedoIndex] = textureID;

		return true;
	}

	bool MaterialImporter::CreateNormalHeight(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const
	{
		if (desc.normalHeightID != 0)
		{
			material.textures[MaterialConstants::c_PbrNormalHeightIndex] = desc.normalHeightID;
			return true;
		}

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

		// Use the highest bit depth of any of the textures
		const ImageBitDepth bitDepth = static_cast<ImageBitDepth>(std::max(static_cast<uint8>(normalInfo.bitDepth),
			static_cast<uint8>(heightInfo.bitDepth)));

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
			compDesc.image = normal;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_32F;
			break;
		}
		default:
			return false;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, "_NormalHeight", AssetConstants::c_TextureFileExtension);

		compDesc.width = normalInfo.width;
		compDesc.height = normalInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = RenderConstants::c_MaxMips;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = desc.isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		const AssetID textureID = AssetUtil::CreateAssetID();

		AssetRegistry::Instance().AddAsset(textureID, compDesc.outputFilePath, &materialID, 1);

		material.textures[MaterialConstants::c_PbrNormalHeightIndex] = textureID;

		return true;
	}

	bool MaterialImporter::CreateAORoughnessMetallic(const PbrMaterialImportDesc& desc, AssetID materialID, MaterialAssetFile& material) const
	{
		if (desc.aoRoughnessMetallicID != 0)
		{
			material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = desc.aoRoughnessMetallicID;
			return true;
		}

		// Occlusion and metallicRoughness are commonly the same texture (R=occlusion, G=roughness,
		// B=metallic - the same packing this importer outputs), in which case there's nothing to
		// merge and no reason to decode the image twice.
		const bool sameSource = IsSameTextureSource(desc.occlusionSource, desc.roughnessMetallicSource);

		ImageInfo roughnessMetallicInfo;
		LoadImageInfo(desc.roughnessMetallicSource, roughnessMetallicInfo);

		ImageInfo aoInfo;
		if (sameSource)
		{
			aoInfo = roughnessMetallicInfo;
		}
		else
		{
			LoadImageInfo(desc.occlusionSource, aoInfo);

			if (aoInfo.width != roughnessMetallicInfo.width || aoInfo.height != roughnessMetallicInfo.height)
			{
				TYR_LOG_ERROR("The dimensions of the occlusion and roughnessMetallic texture does not match for the PBR material %s.", desc.materialName);
				return false;
			}
		}

		const uint minReqRoughnessMetallicChannelCount = 3;
		if (roughnessMetallicInfo.channelCount < minReqRoughnessMetallicChannelCount)
		{
			TYR_LOG_ERROR("The channel count of the metallic texture is less than required %ud for the PBR material %s.", minReqRoughnessMetallicChannelCount, desc.materialName);
			return false;
		}

		// Use the highest bit depth of any of the textures
		const ImageBitDepth bitDepth = static_cast<ImageBitDepth>(std::max(static_cast<uint8>(aoInfo.bitDepth),
			static_cast<uint8>(roughnessMetallicInfo.bitDepth)));

		Image2DCompressionDesc compDesc;

		const uint texelCount = roughnessMetallicInfo.width * roughnessMetallicInfo.height;
		if (bitDepth == ImageBitDepth::EightBit)
		{
			uint8* roughnessMetallic = LoadImage8U(desc.roughnessMetallicSource, 4);
			if (!sameSource)
			{
				uint8* ao = LoadImage8U(desc.occlusionSource, 1);
				// Write ao to channel 0 of combined texture
				ImageUtil::CopyChannel<uint8>(ao, roughnessMetallic, texelCount, 1, 4, 0, 0);
				ImageLoader::FreeImage(ao);
			}
			compDesc.image = roughnessMetallic;
			compDesc.inputFormat = ImageCompressionInputFormat::RGBA_8U;
		}
		else
		{
			float* roughnessMetallic = LoadImage32F(desc.roughnessMetallicSource, 4);
			if (!sameSource)
			{
				float* ao = LoadImage32F(desc.occlusionSource, 1);
				// Write ao to channel 0 of combined texture
				ImageUtil::CopyChannel<float>(ao, roughnessMetallic, texelCount, 1, 4, 0, 0);
				ImageLoader::FreeImage(ao);
			}
			compDesc.image = roughnessMetallic;
			compDesc.inputFormat = bitDepth == ImageBitDepth::SixteenBit ? ImageCompressionInputFormat::RGBA_16F : ImageCompressionInputFormat::RGBA_32F;
		}

		char outputPath[TYR_MAX_PATH_TOTAL_SIZE];
		snprintf(outputPath, sizeof(outputPath), "%s/%s%s%s", desc.outputFolderPath, desc.materialName, "_AORoughnessMetallic", AssetConstants::c_TextureFileExtension);

		compDesc.width = roughnessMetallicInfo.width;
		compDesc.height = roughnessMetallicInfo.height;
		compDesc.outputFormat = ImageCompressionOutputFormat::BC7;
		compDesc.mipCount = RenderConstants::c_MaxMips;
		compDesc.outputFilePath = outputPath;
		compDesc.isSRGB = desc.isSRGB;

		const bool compressResult = ImageCompressor::CompressImage2D(compDesc);

		ImageLoader::FreeImage(compDesc.image);

		if (!compressResult)
		{
			return false;
		}

		const AssetID textureID = AssetUtil::CreateAssetID();

		AssetRegistry::Instance().AddAsset(textureID, compDesc.outputFilePath, &materialID, 1);

		material.textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex] = textureID;

		return true;
	}

	bool MaterialImporter::CreateMaterialAssetInfo(const char* outputFolderPath, const char* materialPath, MaterialAssetFile& material) const
	{
		char absMaterialFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMaterialFolderPath, outputFolderPath);

		bool exists;
		const int refCount = AssetRegistry::Instance().GetAssetReferenceCount(materialPath);

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
			if (!fs::create_directories(absMaterialFolderPath, ec))
			{
				TYR_LOG_ERROR("Error creating directory %s.", absMaterialFolderPath);
				return false;
			}
		}

		return true;
	}

	bool MaterialImporter::ImportPbrMaterial(const PbrMaterialImportDesc& desc) const
	{
		MaterialAssetFile material;
		material.type = MaterialType::PBR;

		char materialPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(materialPath, sizeof(materialPath), "%s/%s%s", desc.outputFolderPath, desc.materialName, AssetConstants::c_MaterialFileExtension);

		const AssetID materialID = AssetUtil::CreateAssetID();

		if (!CreateMaterialAssetInfo(desc.outputFolderPath, materialPath, material))
		{
			return false;
		}

		if (!CreateAlbedo(desc, materialID, material) || !CreateNormalHeight(desc, materialID, material) || !CreateAORoughnessMetallic(desc, materialID, material))
		{
			return false;
		}

		AssetRegistry::Instance().AddAsset(materialID, materialPath, material.textures.Data(), material.textures.Size());

		return SerializeMaterial(desc, material);
	}

	bool MaterialImporter::SerializeMaterial(const PbrMaterialImportDesc& desc, const MaterialAssetFile& material) const
	{
		char materialPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(materialPath, sizeof(materialPath), "%s/%s%s", desc.outputFolderPath, desc.materialName, AssetConstants::c_MaterialFileExtension);

		AssetUtil::SaveAsset<MaterialAssetFile>(materialPath, material);

		return true;
	}
}

	