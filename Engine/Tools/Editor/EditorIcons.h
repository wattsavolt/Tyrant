#pragma once

#include "EditorMacros.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	class AssetManager;

	// The icon textures used across the editor's UI, from PNGs in SourceAssets/Editor.
	class TYR_EDITOR_EXPORT EditorIcons final
	{
	public:
		enum Icon : uint8
		{
			Play,
			Pause,
			Stop,
			Folder,
			IconCount
		};

		// Imports every icon from SourceAssets.
		static void Import();

		static const char* GetName(Icon icon);

		// Starts loading the icons, which must already have been imported.
		EditorIcons(AssetManager& assetManager);
		~EditorIcons();

		// Returns 0 until the icon has loaded.
		uint64 GetImTextureID(Icon icon) const;

	private:
		AssetManager& m_AssetManager;
		AssetID m_Icons[IconCount];
	};
}
