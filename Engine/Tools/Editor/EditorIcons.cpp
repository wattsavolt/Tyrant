#include "EditorIcons.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "Importing/MaterialImporter.h"
#include <cstdio>

namespace tyr
{
	namespace
	{
		constexpr const char* c_IconFolder = "Editor";
		// Indexed by EditorIcons::Icon, matching each icon's PNG name.
		constexpr const char* c_IconNames[] = { "Play", "Pause", "Stop", "Folder" };
		static_assert(sizeof(c_IconNames) / sizeof(c_IconNames[0]) == EditorIcons::IconCount);
	}

	void EditorIcons::Import()
	{
		for (const char* name : c_IconNames)
		{
			char sourceRelativePath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(sourceRelativePath, sizeof(sourceRelativePath), "%s/%s.png", c_IconFolder, name);
			char sourcePath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullSourceAssetPath(sourcePath, sourceRelativePath);

			// Not sRGB, to match how the rest of the UI's colours and textures are treated.
			AssetID iconID;
			if (!MaterialImporter::Instance().ImportTexture(TextureSource{ sourcePath }, c_IconFolder, name, false, iconID))
			{
				TYR_LOG_ERROR("Failed to import editor icon %s.", sourcePath);
			}
		}
	}

	const char* EditorIcons::GetName(Icon icon)
	{
		return c_IconNames[icon];
	}

	EditorIcons::EditorIcons(AssetManager& assetManager)
		: m_AssetManager(assetManager)
	{
		for (uint i = 0; i < IconCount; ++i)
		{
			char assetPath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(assetPath, sizeof(assetPath), "%s/%s%s", c_IconFolder, c_IconNames[i], AssetConstants::c_TextureFileExtension);

			m_Icons[i] = AssetRegistry::Instance().GetAssetID(assetPath);
			if (AssetUtil::IsValidAssetID(m_Icons[i]))
			{
				m_AssetManager.LoadTexture(m_Icons[i]);
			}
			else
			{
				TYR_LOG_WARNING("Editor icon %s hasn't been imported.", assetPath);
			}
		}
	}

	EditorIcons::~EditorIcons()
	{
		for (AssetID iconID : m_Icons)
		{
			if (AssetUtil::IsValidAssetID(iconID))
			{
				m_AssetManager.DeleteTexture(iconID);
			}
		}
	}

	uint64 EditorIcons::GetImTextureID(Icon icon) const
	{
		const TextureHandle texture = m_AssetManager.GetTexture(m_Icons[icon]);
		// +1 because ImGui treats a texture ID of 0 as unset.
		return texture ? static_cast<uint64>(texture.h.index) + 1 : 0;
	}
}
