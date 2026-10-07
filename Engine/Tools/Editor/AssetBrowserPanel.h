#pragma once

#include "EditorMacros.h"
#include "AssetSystem/AssetID.h"
#include "String/Path.h"

namespace tyr
{
	class AssetManager;
	class EditorIcons;

	// Bottom panel for browsing, renaming, moving and deleting the project's assets. Renaming and
	// moving keep an asset's ID, so everything referencing it still finds it.
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
		// Changes chosen from an asset's menu, applied once the grid is drawn since they change
		// the lists being drawn.
		struct PendingAssetEdit
		{
			AssetID assetToDelete;
			AssetID assetToMove;
			// Where to move it, empty for the root folder.
			RelativePath moveFolder;
		};

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
		void DrawAssetTile(AssetID assetID, PendingAssetEdit& edit);
		void DrawAssetMenu(AssetID assetID, const char* path, PendingAssetEdit& edit);
		// Draws the rename box over the name of the asset being renamed, if it's on screen.
		void DrawRenameBox();

		// Moves an asset dropped onto the last drawn item into the folder.
		void AcceptAssetDrop(const char* folderPath);
		void MoveAsset(AssetID assetID, const char* folderPath);
		// newName has no extension, since it keeps its own.
		void RenameAsset(AssetID assetID, const char* newName);
		// Moves the asset's file to newPath and points its registry entry there.
		void RelocateAsset(AssetID assetID, const char* newPath);
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

		// The asset being renamed in place, if any, and where its tile was drawn this frame.
		AssetID m_RenamingAsset;
		bool m_RenameFocusPending = false;
		bool m_RenameTileDrawn = false;
		float m_RenameTileX = 0.0f;
		float m_RenameTileY = 0.0f;
		char m_RenameBuffer[PathConstants::c_MaxFileNameTotalSize] = {};
	};
}
