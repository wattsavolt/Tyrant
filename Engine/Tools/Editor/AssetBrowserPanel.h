#pragma once

#include "EditorMacros.h"
#include "AssetSystem/AssetID.h"
#include "String/Path.h"

namespace tyr
{
	class AssetManager;
	class EditorIcons;

	// Bottom panel for browsing, moving and deleting the project's assets.
	class TYR_EDITOR_EXPORT AssetBrowserPanel final
	{
	public:
		// Drag and drop payload types, both carrying an AssetID. Meshes have their own type so
		// the viewport only accepts meshes.
		static constexpr const char* c_MeshPayload = "TyrMeshAsset";
		static constexpr const char* c_AssetPayload = "TyrAsset";

		AssetBrowserPanel(AssetManager& assetManager, const EditorIcons& icons);

		// Draws the panel's contents into the current window.
		void Draw();

	private:
		// Rebuilds the folder and asset lists when the registry has changed.
		void RefreshIfNeeded();
		void RebuildFolders();
		void RebuildFolderContents();
		// reveal opens the folder's parents in the tree so it can be seen there.
		void SelectFolder(const char* folderPath, bool reveal);

		void DrawFolderTree();
		// Returns true when the node is open.
		bool DrawFolderNode(const char* label, const char* folderPath, bool isLeaf, bool defaultOpen);
		void DrawGrid();
		// Returns the folder to open when the tile is double clicked, otherwise null.
		const char* DrawFolderTile(uint folderIndex);
		// Returns true when Delete was chosen from the tile's menu.
		bool DrawAssetTile(AssetID assetID);

		// Moves an asset dropped onto the last drawn item into the folder.
		void AcceptAssetDrop(const char* folderPath);
		void MoveAsset(AssetID assetID, const char* folderPath);
		void DeleteAsset(AssetID assetID);

		AssetManager& m_AssetManager;
		const EditorIcons& m_Icons;
		// Every folder holding an asset, sorted so each folder comes straight after its parent.
		Array<RelativePath> m_Folders;
		// Indices into m_Folders of the folders directly inside the selected folder.
		Array<uint> m_Subfolders;
		// The assets directly in the selected folder, sorted by path.
		Array<AssetID> m_FolderAssets;
		// Empty for the root assets folder.
		RelativePath m_SelectedFolder;
		AssetID m_SelectedAsset;
		uint m_RegistryVersion = 0;
		bool m_RevealSelectedFolder = false;
	};
}
