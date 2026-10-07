#include "AssetBrowserPanel.h"
#include "EditorIcons.h"
#include "EditorPathUtil.h"
#include "EditorWidgets.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "Utility/PathUtil.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr float c_FolderTreeWidth = 260.0f;
		// Characters Windows doesn't allow in file names, plus the path separator.
		constexpr const char* c_InvalidFileNameCharacters = "\\/:*?\"<>|";
		constexpr float c_TileSize = 72.0f;
		constexpr float c_TilePadding = 8.0f;
		constexpr float c_TileRounding = 4.0f;
		// Label size relative to the normal font, shrunk further when the label doesn't fit.
		constexpr float c_TileLabelScale = 1.6f;

		enum class AssetKind : uint8
		{
			Texture,
			Material,
			Mesh,
			Other
		};

		struct AssetKindStyle
		{
			const char* label;
			ImU32 colour;
		};

		// Indexed by AssetKind.
		const AssetKindStyle c_AssetKindStyles[] =
		{
			{ "Tex", IM_COL32(196, 82, 70, 255) },
			{ "Mat", IM_COL32(146, 92, 214, 255) },
			{ "Mesh", IM_COL32(84, 168, 92, 255) },
			{ "?", IM_COL32(110, 110, 110, 255) }
		};

		AssetKind GetAssetKind(const char* path)
		{
			const char* extension = strrchr(path, '.');
			if (!extension)
			{
				return AssetKind::Other;
			}
			if (strcmp(extension, AssetConstants::c_TextureFileExtension) == 0)
			{
				return AssetKind::Texture;
			}
			if (strcmp(extension, AssetConstants::c_MaterialFileExtension) == 0)
			{
				return AssetKind::Material;
			}
			if (strcmp(extension, AssetConstants::c_MeshFileExtension) == 0)
			{
				return AssetKind::Mesh;
			}
			return AssetKind::Other;
		}

		// Adds an invisible button covering the tile and its name, returning the tile's top left.
		ImVec2 AddTileItem()
		{
			const ImVec2 min = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##Tile", ImVec2(c_TileSize, c_TileSize + ImGui::GetTextLineHeightWithSpacing()));
			return min;
		}

		void DrawTileOutline(const ImVec2& min, bool selected, bool hovered)
		{
			if (selected || hovered)
			{
				const ImU32 colour = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 160);
				ImGui::GetWindowDrawList()->AddRect(min, ImVec2(min.x + c_TileSize, min.y + c_TileSize), colour, c_TileRounding, 0, 2.0f);
			}
		}

		// The name is centred under the tile and clipped to its width.
		void DrawTileName(const ImVec2& min, const char* name)
		{
			ImFont* font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize();
			const float nameWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name).x;
			const float x = min.x + std::max(0.0f, (c_TileSize - nameWidth) * 0.5f);
			const float y = min.y + c_TileSize;
			const ImVec4 clipRect(min.x, y, min.x + c_TileSize, y + ImGui::GetTextLineHeightWithSpacing());
			ImGui::GetWindowDrawList()->AddText(font, fontSize, ImVec2(x, y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), name, nullptr, 0.0f, &clipRect);
		}

		void DrawAssetTileBody(const ImVec2& min, AssetKind kind)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const AssetKindStyle& style = c_AssetKindStyles[static_cast<uint8>(kind)];
			drawList->AddRectFilled(min, ImVec2(min.x + c_TileSize, min.y + c_TileSize), style.colour, c_TileRounding);

			// Shrunk to fit inside the tile's padding when it's too wide at the normal scale.
			ImFont* font = ImGui::GetFont();
			const float maxWidth = c_TileSize - c_TilePadding * 2.0f;
			float labelSize = ImGui::GetFontSize() * c_TileLabelScale;
			ImVec2 extent = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, style.label);
			if (extent.x > maxWidth)
			{
				labelSize *= maxWidth / extent.x;
				extent = font->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, style.label);
			}
			drawList->AddText(font, labelSize, ImVec2(min.x + (c_TileSize - extent.x) * 0.5f, min.y + (c_TileSize - extent.y) * 0.5f),
				IM_COL32(255, 255, 255, 255), style.label);
		}
	}

	AssetBrowserPanel::AssetBrowserPanel(AssetManager& assetManager, const EditorIcons& icons)
		: m_AssetManager(assetManager)
		, m_Icons(icons)
	{
	}

	void AssetBrowserPanel::Draw()
	{
		RefreshIfNeeded();

		ImGui::BeginChild("##Folders", ImVec2(c_FolderTreeWidth, 0.0f), ImGuiChildFlags_Borders);
		DrawFolderTree();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##Assets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		DrawGrid();
		ImGui::EndChild();
	}

	void AssetBrowserPanel::RefreshIfNeeded()
	{
		const uint version = AssetRegistry::Instance().GetVersion();
		if (version == m_RegistryVersion)
		{
			return;
		}
		m_RegistryVersion = version;
		RebuildFolders();
		RebuildFolderContents();
	}

	void AssetBrowserPanel::RebuildFolders()
	{
		m_Folders.Clear();
		for (std::pair<const AssetID&, const RegAssetData&> asset : AssetRegistry::Instance().GetAssets())
		{
			const char* path = asset.second.filePath.CStr();
			for (const char* c = path; *c != '\0'; ++c)
			{
				if (*c == '/')
				{
					m_Folders.Add(RelativePath(path, static_cast<size_t>(c - path)));
				}
			}
		}

		std::sort(m_Folders.begin(), m_Folders.end(), [](const RelativePath& a, const RelativePath& b)
		{
			return EditorPathUtil::PathLess(a.CStr(), b.CStr());
		});

		// Removes duplicates, which are next to each other after sorting.
		uint uniqueCount = 0;
		for (uint i = 0; i < m_Folders.Size(); ++i)
		{
			if (uniqueCount == 0 || !(m_Folders[i] == m_Folders[uniqueCount - 1]))
			{
				m_Folders[uniqueCount++] = m_Folders[i];
			}
		}
		m_Folders.Resize(uniqueCount);
	}

	void AssetBrowserPanel::RebuildFolderContents()
	{
		const AssetRegistry& registry = AssetRegistry::Instance();
		const char* folderPath = m_SelectedFolder.CStr();
		const size_t folderLength = strlen(folderPath);

		// Already in order since m_Folders is sorted.
		m_Subfolders.Clear();
		for (uint i = 0; i < m_Folders.Size(); ++i)
		{
			if (EditorPathUtil::IsDirectlyInFolder(m_Folders[i].CStr(), folderPath, folderLength))
			{
				m_Subfolders.Add(i);
			}
		}

		m_FolderAssets.Clear();
		for (std::pair<const AssetID&, const RegAssetData&> asset : registry.GetAssets())
		{
			if (EditorPathUtil::IsDirectlyInFolder(asset.second.filePath.CStr(), folderPath, folderLength))
			{
				m_FolderAssets.Add(asset.first);
			}
		}

		std::sort(m_FolderAssets.begin(), m_FolderAssets.end(), [&registry](AssetID a, AssetID b)
		{
			return EditorPathUtil::PathLess(registry.GetAssetData(a).filePath.CStr(), registry.GetAssetData(b).filePath.CStr());
		});
	}

	void AssetBrowserPanel::SelectFolder(const char* folderPath, bool reveal)
	{
		if (m_SelectedFolder == folderPath)
		{
			return;
		}
		m_SelectedFolder = folderPath;
		m_RevealSelectedFolder = reveal;
		m_RenamingAsset = {};
		RebuildFolderContents();
	}

	void AssetBrowserPanel::DrawFolderTree()
	{
		if (DrawFolderNode("Assets", "", m_Folders.IsEmpty(), true))
		{
			// Nodes at this depth or above are visible, and this many tree nodes are pushed.
			uint openDepth = 1;
			for (uint i = 0; i < m_Folders.Size(); ++i)
			{
				const char* folderPath = m_Folders[i].CStr();
				const uint depth = EditorPathUtil::GetFolderDepth(folderPath);
				if (depth > openDepth)
				{
					continue;
				}
				for (; openDepth > depth; --openDepth)
				{
					ImGui::TreePop();
				}

				const bool hasSubfolders = i + 1 < m_Folders.Size() && EditorPathUtil::IsSubfolder(m_Folders[i + 1].CStr(), folderPath);
				if (DrawFolderNode(EditorPathUtil::GetLastPathPart(folderPath), folderPath, !hasSubfolders, false) && hasSubfolders)
				{
					++openDepth;
				}
			}

			for (; openDepth > 0; --openDepth)
			{
				ImGui::TreePop();
			}
		}

		m_RevealSelectedFolder = false;
	}

	bool AssetBrowserPanel::DrawFolderNode(const char* label, const char* folderPath, bool isLeaf, bool defaultOpen)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (isLeaf)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (defaultOpen)
		{
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		if (m_SelectedFolder == folderPath)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// Opens the parents of a folder that was opened from the grid.
		const bool isRoot = folderPath[0] == '\0';
		if (m_RevealSelectedFolder && !isLeaf && (isRoot || EditorPathUtil::IsSubfolder(m_SelectedFolder.CStr(), folderPath)))
		{
			ImGui::SetNextItemOpen(true);
		}

		// The root's path is empty, so it needs an ID of its own.
		const bool open = ImGui::TreeNodeEx(isRoot ? "##AssetsRoot" : folderPath, flags, "%s", label);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			SelectFolder(folderPath, false);
		}
		AcceptAssetDrop(folderPath);
		return open && !isLeaf;
	}

	void AssetBrowserPanel::DrawGrid()
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float available = ImGui::GetContentRegionAvail().x + style.ItemSpacing.x;
		const uint columns = std::max(1u, static_cast<uint>(available / (c_TileSize + style.ItemSpacing.x)));

		// Opening, moving and deleting are left until after the loops since they change the lists
		// being drawn.
		const char* folderToOpen = nullptr;
		PendingAssetEdit edit;
		m_RenameTileDrawn = false;
		uint tileIndex = 0;

		for (uint folderIndex : m_Subfolders)
		{
			if (tileIndex++ % columns != 0)
			{
				ImGui::SameLine();
			}
			ImGui::PushID(static_cast<int>(tileIndex));
			if (const char* folderPath = DrawFolderTile(folderIndex))
			{
				folderToOpen = folderPath;
			}
			ImGui::PopID();
		}

		for (AssetID assetID : m_FolderAssets)
		{
			if (tileIndex++ % columns != 0)
			{
				ImGui::SameLine();
			}
			ImGui::PushID(static_cast<int>(tileIndex));
			DrawAssetTile(assetID, edit);
			ImGui::PopID();
		}

		// Drawn after the tiles so it doesn't shift where the next tile goes.
		DrawRenameBox();

		if (folderToOpen)
		{
			SelectFolder(folderToOpen, true);
		}
		else if (AssetUtil::IsValidAssetID(edit.assetToDelete))
		{
			DeleteAsset(edit.assetToDelete);
		}
		else if (AssetUtil::IsValidAssetID(edit.assetToMove))
		{
			MoveAsset(edit.assetToMove, edit.moveFolder.CStr());
		}
	}

	const char* AssetBrowserPanel::DrawFolderTile(uint folderIndex)
	{
		const char* folderPath = m_Folders[folderIndex].CStr();
		const ImVec2 min = AddTileItem();
		const bool hovered = ImGui::IsItemHovered();
		const bool open = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		ImGui::SetItemTooltip("%s", folderPath);
		AcceptAssetDrop(folderPath);

		const ImTextureID textureID = m_Icons.GetImTextureID(EditorIcons::Folder);
		if (textureID)
		{
			ImGui::GetWindowDrawList()->AddImage(textureID, min, ImVec2(min.x + c_TileSize, min.y + c_TileSize));
		}
		DrawTileOutline(min, false, hovered);
		DrawTileName(min, EditorPathUtil::GetLastPathPart(folderPath));

		return open ? folderPath : nullptr;
	}

	void AssetBrowserPanel::DrawAssetTile(AssetID assetID, PendingAssetEdit& edit)
	{
		const char* path = AssetRegistry::Instance().GetAssetData(assetID).filePath.CStr();
		const AssetKind kind = GetAssetKind(path);
		char name[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(path, name);

		const ImVec2 min = AddTileItem();
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_SelectedAsset = assetID;
		}
		ImGui::SetItemTooltip("%s", path);

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(kind == AssetKind::Mesh ? c_MeshPayload : c_AssetPayload, &assetID, sizeof(AssetID));
			ImGui::TextUnformatted(name);
			ImGui::EndDragDropSource();
		}

		DrawAssetMenu(assetID, path, edit);

		DrawAssetTileBody(min, kind);
		DrawTileOutline(min, assetID == m_SelectedAsset, hovered);
		if (assetID == m_RenamingAsset)
		{
			// The rename box covers the name once the grid is drawn.
			m_RenameTileDrawn = true;
			m_RenameTileX = min.x;
			m_RenameTileY = min.y;
		}
		else
		{
			DrawTileName(min, name);
		}
	}

	void AssetBrowserPanel::DrawAssetMenu(AssetID assetID, const char* path, PendingAssetEdit& edit)
	{
		if (!ImGui::BeginPopupContextItem("##TileMenu"))
		{
			return;
		}

		if (ImGui::MenuItem("Rename"))
		{
			m_RenamingAsset = assetID;
			m_RenameFocusPending = true;
			PathUtil::GetFileNameWithoutExtension(path, m_RenameBuffer);
		}

		// Every other folder, starting with the root.
		if (ImGui::BeginMenu("Move To"))
		{
			const char* currentFolderEnd = strrchr(path, '/');
			const RelativePath currentFolder = currentFolderEnd ? RelativePath(path, static_cast<size_t>(currentFolderEnd - path)) : RelativePath();
			if (ImGui::MenuItem("Assets", nullptr, false, currentFolder.Size() != 0))
			{
				edit.assetToMove = assetID;
				edit.moveFolder = {};
			}
			for (const RelativePath& folder : m_Folders)
			{
				if (ImGui::MenuItem(folder.CStr(), nullptr, false, !(folder == currentFolder)))
				{
					edit.assetToMove = assetID;
					edit.moveFolder = folder;
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Delete"))
		{
			edit.assetToDelete = assetID;
		}
		ImGui::EndPopup();
	}

	void AssetBrowserPanel::DrawRenameBox()
	{
		if (!AssetUtil::IsValidAssetID(m_RenamingAsset) || !m_RenameTileDrawn)
		{
			return;
		}

		ImGui::SetCursorScreenPos(ImVec2(m_RenameTileX, m_RenameTileY + c_TileSize));
		const EditorWidgets::RenameResult result = EditorWidgets::DrawRenameBox(m_RenameBuffer, sizeof(m_RenameBuffer), m_RenameFocusPending, c_TileSize);
		if (result == EditorWidgets::RenameResult::Committed)
		{
			RenameAsset(m_RenamingAsset, m_RenameBuffer);
		}
		if (result != EditorWidgets::RenameResult::Editing)
		{
			m_RenamingAsset = {};
		}
	}

	void AssetBrowserPanel::AcceptAssetDrop(const char* folderPath)
	{
		if (!ImGui::BeginDragDropTarget())
		{
			return;
		}

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(c_AssetPayload);
		if (!payload)
		{
			payload = ImGui::AcceptDragDropPayload(c_MeshPayload);
		}
		if (payload)
		{
			MoveAsset(*static_cast<const AssetID*>(payload->Data), folderPath);
		}
		ImGui::EndDragDropTarget();
	}

	void AssetBrowserPanel::MoveAsset(AssetID assetID, const char* folderPath)
	{
		const char* fileName = EditorPathUtil::GetLastPathPart(AssetRegistry::Instance().GetAssetData(assetID).filePath.CStr());

		char newPath[PathConstants::c_MaxAssetPathTotalSize];
		const int length = folderPath[0] != '\0'
			? snprintf(newPath, sizeof(newPath), "%s/%s", folderPath, fileName)
			: snprintf(newPath, sizeof(newPath), "%s", fileName);
		if (length < 0 || length >= static_cast<int>(sizeof(newPath)))
		{
			TYR_LOG_WARNING("Can't move %s, the new path would be too long.", fileName);
			return;
		}
		RelocateAsset(assetID, newPath);
	}

	void AssetBrowserPanel::RenameAsset(AssetID assetID, const char* newName)
	{
		if (newName[0] == '\0' || strpbrk(newName, c_InvalidFileNameCharacters))
		{
			TYR_LOG_WARNING("Can't rename an asset to \"%s\", names can't be empty or use any of %s", newName, c_InvalidFileNameCharacters);
			return;
		}

		// Stays in the same folder with the same extension.
		const char* oldPath = AssetRegistry::Instance().GetAssetData(assetID).filePath.CStr();
		const char* fileName = EditorPathUtil::GetLastPathPart(oldPath);
		const char* extension = strrchr(fileName, '.');
		const int folderLength = static_cast<int>(fileName - oldPath);

		char newPath[PathConstants::c_MaxAssetPathTotalSize];
		const int length = snprintf(newPath, sizeof(newPath), "%.*s%s%s", folderLength, oldPath, newName, extension ? extension : "");
		if (length < 0 || length >= static_cast<int>(sizeof(newPath)))
		{
			TYR_LOG_WARNING("Can't rename %s, the new path would be too long.", oldPath);
			return;
		}
		RelocateAsset(assetID, newPath);
	}

	void AssetBrowserPanel::RelocateAsset(AssetID assetID, const char* newPath)
	{
		// Its file is still being read.
		if (m_AssetManager.GetLoadState(assetID) == AssetLoadState::Loading)
		{
			TYR_LOG_WARNING("Can't move or rename an asset while it's loading.");
			return;
		}

		AssetRegistry& registry = AssetRegistry::Instance();
		const AssetPath oldPath = registry.GetAssetData(assetID).filePath;
		if (oldPath == newPath)
		{
			return;
		}

		char absOldPath[TYR_MAX_PATH_TOTAL_SIZE];
		char absNewPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absOldPath, oldPath.CStr());
		AssetUtil::CreateFullPath(absNewPath, newPath);
		if (AssetUtil::IsValidAssetID(registry.GetAssetID(newPath)) || fs::exists(absNewPath))
		{
			TYR_LOG_WARNING("Can't move %s, %s already exists.", oldPath.CStr(), newPath);
			return;
		}

		// The destination folder may not exist yet on disk.
		std::error_code ec;
		fs::create_directories(fs::path(absNewPath).parent_path(), ec);
		fs::rename(absOldPath, absNewPath, ec);
		if (ec)
		{
			TYR_LOG_ERROR("Failed to move %s to %s.", absOldPath, absNewPath);
			return;
		}

		registry.UpdateAssetPath(assetID, newPath);
		registry.Save();
	}

	void AssetBrowserPanel::DeleteAsset(AssetID assetID)
	{
		AssetRegistry& registry = AssetRegistry::Instance();
		const AssetPath path = registry.GetAssetData(assetID).filePath;

		m_AssetManager.EvictUnreferencedAsset(assetID);
		if (m_AssetManager.GetLoadState(assetID) != AssetLoadState::Unloaded)
		{
			TYR_LOG_WARNING("Can't delete %s while it's in use.", path.CStr());
			return;
		}
		if (registry.GetAssetReferenceCount(path.CStr()) > 0)
		{
			TYR_LOG_WARNING("Can't delete %s while other assets use it.", path.CStr());
			return;
		}

		char absPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absPath, path.CStr());
		std::error_code ec;
		fs::remove(absPath, ec);
		if (ec)
		{
			TYR_LOG_ERROR("Failed to delete %s.", absPath);
			return;
		}

		registry.RemoveAsset(assetID);
		registry.Save();
		if (m_SelectedAsset == assetID)
		{
			m_SelectedAsset = {};
		}
	}
}
