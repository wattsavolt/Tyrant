#include "FontImporter.h"
#include "AssetSystem/FontAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetConstants.h"
#include "IO/FileStream.h"
#include "IO/BufferedFileStream.h"
#include "Memory/StackAllocation.h"
#include "Utility/PathUtil.h"
#include "Platform/Platform.h"

namespace tyr
{
	namespace
	{
		// Larger files are almost certainly not a font meant for the UI.
		constexpr size_t c_MaxFontFileSize = 16 * 1024 * 1024;

		// TrueType files start with version 1.0 or "true", OpenType ones with "OTTO".
		bool IsFontFile(const Array<uint8>& data)
		{
			if (data.Size() < 12)
			{
				return false;
			}
			const uint8* tag = data.Data();
			return (tag[0] == 0 && tag[1] == 1 && tag[2] == 0 && tag[3] == 0)
				|| (tag[0] == 't' && tag[1] == 'r' && tag[2] == 'u' && tag[3] == 'e')
				|| (tag[0] == 'O' && tag[1] == 'T' && tag[2] == 'T' && tag[3] == 'O');
		}
	}

	bool FontImporter::ImportFont(const char* sourcePath, const char* outputFolder, const char* name, AssetID& outFontID)
	{
		if (!Platform::FileExists(sourcePath))
		{
			TYR_LOG_ERROR("There's no font file at %s.", sourcePath);
			return false;
		}
		const size_t size = FileStream::GetFileSize(sourcePath);
		if (size == 0 || size > c_MaxFontFileSize)
		{
			TYR_LOG_ERROR("%s is empty or too big to be a font.", sourcePath);
			return false;
		}

		FontFile font;
		font.data.Resize(static_cast<uint>(size));
		FileStream::ReadAllFile(sourcePath, font.data);
		if (!IsFontFile(font.data))
		{
			TYR_LOG_ERROR("%s isn't a TrueType or OpenType font.", sourcePath);
			return false;
		}

		char absFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absFolderPath, outputFolder);
		if (!PathUtil::CreateDirectories(absFolderPath))
		{
			TYR_LOG_ERROR("Error creating directory %s.", absFolderPath);
			return false;
		}

		char assetPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(assetPath, sizeof(assetPath), "%s/%s%s", outputFolder, name, AssetConstants::c_FontFileExtension);
		char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absFilePath, assetPath);
		{
			constexpr size_t c_StreamBufferSize = 65536;
			SmartStack<uint8> streamBuffer = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
			BufferedFileStream stream(streamBuffer, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Write, true);
			Serialize<FontFile>(stream, font);
		}

		outFontID = AssetRegistry::Instance().AddOrUpdateAsset(assetPath);
		TYR_LOG_INFO("Imported font %s (%u KB).", assetPath, font.data.Size() / 1024);
		return true;
	}
}
