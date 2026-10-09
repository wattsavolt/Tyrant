#include "LevelHierarchyPanel.h"
#include "EditorIcons.h"
#include "EditorPathUtil.h"
#include "EditorWidgets.h"
#include "AssetBrowserPanel.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr const char* c_NewFolderName = "New Folder";
		constexpr const char* c_AddActorName = "Add Actor";
		// How many actor types the Add Actor popup lists before scrolling, and how wide it is.
		constexpr float c_ActorPickerRows = 10.0f;
		constexpr float c_ActorPickerWidthInFontSizes = 18.0f;

		// Adds the folder and every folder above it.
		void AddFolderWithParents(Array<RelativePath>& folders, const char* path)
		{
			for (const char* c = path; *c != '\0'; ++c)
			{
				if (*c == '/')
				{
					folders.Add(RelativePath(path, static_cast<size_t>(c - path)));
				}
			}
			if (path[0] != '\0')
			{
				folders.Add(RelativePath(path));
			}
		}

		// The folder above path, or empty at the top level.
		RelativePath GetParentFolder(const char* path)
		{
			const char* lastSlash = strrchr(path, '/');
			return lastSlash ? RelativePath(path, static_cast<size_t>(lastSlash - path)) : RelativePath();
		}

		// Joins a folder and a name, which may be the root's empty path. Fails if it's too long.
		bool JoinPath(const char* folderPath, const char* name, RelativePath& outPath)
		{
			char path[RelativePath::c_Capacity];
			const int length = folderPath[0] != '\0'
				? snprintf(path, sizeof(path), "%s/%s", folderPath, name)
				: snprintf(path, sizeof(path), "%s", name);
			if (length < 0 || length >= static_cast<int>(sizeof(path)))
			{
				return false;
			}
			outPath = path;
			return true;
		}
	}

	LevelHierarchyPanel::LevelHierarchyPanel(WorldManager& worldManager, Handle levelWorld, const EditorIcons& icons)
		: m_WorldManager(worldManager)
		, m_LevelWorld(levelWorld)
		, m_Icons(icons)
	{
	}

	void LevelHierarchyPanel::Draw(bool editing, Entity& selectedActor, EditorRequests& requests)
	{
		m_Editing = editing;
		World& world = m_WorldManager.GetWorld(m_LevelWorld);
		RebuildIfNeeded(world);

		PendingEdits edits;
		ImGui::BeginChild("##HierarchyTree");

		for (uint i = 0; i < m_Folders.Size();)
		{
			i = DrawFolder(world, i, selectedActor, edits);
		}
		DrawActors(world, 0, m_RootActorCount, selectedActor, edits);

		// The empty space below the tree clears the selection and takes actors back to the root.
		const ImVec2 available = ImGui::GetContentRegionAvail();
		if (available.y > 0.0f)
		{
			ImGui::InvisibleButton("##Empty", ImVec2(std::max(available.x, 1.0f), available.y));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				selectedActor = c_InvalidEntity;
			}
			AcceptDrop("", edits);
			if (editing && ImGui::BeginPopupContextItem("##TreeMenu"))
			{
				if (ImGui::MenuItem(c_AddActorName))
				{
					OpenActorPicker("");
				}
				if (ImGui::MenuItem(c_NewFolderName))
				{
					edits.createFolder = true;
				}
				ImGui::EndPopup();
			}
		}

		ImGui::EndChild();

		if (!editing)
		{
			return;
		}

		DrawActorPicker(requests);
		ApplyEdits(world, selectedActor, edits);
		if (edits.placeMesh)
		{
			requests.placeMesh = true;
			requests.mesh = edits.mesh;
			requests.folder = edits.meshFolder;
		}
	}

	void LevelHierarchyPanel::OpenActorPicker(const char* folderPath)
	{
		m_ActorPickerFolder = folderPath;
		m_OpenActorPicker = true;
	}

	void LevelHierarchyPanel::DrawActorPicker(EditorRequests& requests)
	{
		constexpr const char* c_PopupID = "##AddActorPopup";
		const bool opening = m_OpenActorPicker;
		if (opening)
		{
			ImGui::OpenPopup(c_PopupID);
			m_OpenActorPicker = false;
		}

		ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * c_ActorPickerWidthInFontSizes, 0.0f));
		if (!ImGui::BeginPopup(c_PopupID))
		{
			return;
		}

		ImGui::TextDisabled("%s", c_AddActorName);
		Id64 actorType;
		if (m_ActorPicker.Draw(actorType, opening, ImGui::GetTextLineHeightWithSpacing() * c_ActorPickerRows))
		{
			requests.placeActor = true;
			requests.actorType = actorType;
			requests.folder = m_ActorPickerFolder;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void LevelHierarchyPanel::DeleteActor(Entity rootEntity, Entity& selectedActor)
	{
		m_WorldManager.RemoveActorInstance(m_LevelWorld, rootEntity);
		if (selectedActor == rootEntity)
		{
			selectedActor = c_InvalidEntity;
		}
		m_Dirty = true;
	}

	void LevelHierarchyPanel::RebuildIfNeeded(const World& world)
	{
		if (!m_Dirty && m_EntitiesVersion == world.entities.GetVersion() && m_ChangeCount == world.changeCount)
		{
			return;
		}
		m_Dirty = false;
		m_EntitiesVersion = world.entities.GetVersion();
		m_ChangeCount = world.changeCount;
		Rebuild(world);
	}

	void LevelHierarchyPanel::Rebuild(const World& world)
	{
		const Array<ActorInstance>& actors = world.actorInstances;

		// Every folder holding an actor or made here, plus the folders above them.
		Array<RelativePath> folderPaths;
		for (const ActorInstance& actor : actors)
		{
			AddFolderWithParents(folderPaths, actor.folderPath.CStr());
		}
		for (const RelativePath& folder : world.folders)
		{
			AddFolderWithParents(folderPaths, folder.CStr());
		}
		std::sort(folderPaths.begin(), folderPaths.end(), [](const RelativePath& a, const RelativePath& b)
		{
			return EditorPathUtil::PathLess(a.CStr(), b.CStr());
		});

		m_Folders.Clear();
		for (const RelativePath& path : folderPaths)
		{
			if (m_Folders.IsEmpty() || !(m_Folders.Back().path == path))
			{
				FolderEntry& folder = m_Folders.ExpandOne();
				folder.path = path;
				folder.firstActor = 0;
				folder.actorCount = 0;
			}
		}

		// Sorted by folder in the same order as m_Folders, so each folder's actors are together.
		m_ActorOrder.Clear();
		for (uint i = 0; i < actors.Size(); ++i)
		{
			m_ActorOrder.Add(i);
		}
		std::sort(m_ActorOrder.begin(), m_ActorOrder.end(), [&actors](uint a, uint b)
		{
			const char* folderA = actors[a].folderPath.CStr();
			const char* folderB = actors[b].folderPath.CStr();
			if (strcmp(folderA, folderB) != 0)
			{
				return EditorPathUtil::PathLess(folderA, folderB);
			}
			return strcmp(actors[a].name.CStr(), actors[b].name.CStr()) < 0;
		});

		m_RootActorCount = 0;
		uint folderIndex = 0;
		for (uint i = 0; i < m_ActorOrder.Size(); ++i)
		{
			const RelativePath& folderPath = actors[m_ActorOrder[i]].folderPath;
			if (folderPath.Size() == 0)
			{
				++m_RootActorCount;
				continue;
			}

			while (!(m_Folders[folderIndex].path == folderPath))
			{
				++folderIndex;
			}
			FolderEntry& folder = m_Folders[folderIndex];
			if (folder.actorCount == 0)
			{
				folder.firstActor = i;
			}
			++folder.actorCount;
		}
	}

	uint LevelHierarchyPanel::DrawFolder(World& world, uint folderIndex, Entity& selectedActor, PendingEdits& edits)
	{
		const FolderEntry& folder = m_Folders[folderIndex];
		const char* path = folder.path.CStr();
		uint next = folderIndex + 1;
		const bool hasSubfolders = next < m_Folders.Size() && EditorPathUtil::IsSubfolder(m_Folders[next].path.CStr(), path);

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
			| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
		if (!hasSubfolders && folder.actorCount == 0)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		ImGui::PushID(path);
		// The label is drawn after the icon below, so the node itself has none.
		const bool open = ImGui::TreeNodeEx("##Folder", flags);
		DrawFolderMenu(path, edits);
		AcceptDrop(path, edits);

		ImGui::SameLine();
		const float iconSize = ImGui::GetTextLineHeight();
		if (const ImTextureID iconID = m_Icons.GetImTextureID(EditorIcons::Folder))
		{
			ImGui::Image(iconID, ImVec2(iconSize, iconSize));
			ImGui::SameLine();
		}

		if (m_RenamingFolder == path)
		{
			const EditorWidgets::RenameResult result = EditorWidgets::DrawRenameBox(m_RenameBuffer, sizeof(m_RenameBuffer), m_RenameFocusPending);
			if (result == EditorWidgets::RenameResult::Committed && m_RenameBuffer[0] != '\0' && !strchr(m_RenameBuffer, '/'))
			{
				RelativePath newPath;
				if (JoinPath(GetParentFolder(path).CStr(), m_RenameBuffer, newPath) && !(newPath == path))
				{
					edits.replaceFolder = true;
					edits.folderToReplace = path;
					edits.replacement = newPath;
				}
			}
			if (result != EditorWidgets::RenameResult::Editing)
			{
				m_RenamingFolder = {};
			}
		}
		else
		{
			ImGui::TextUnformatted(EditorPathUtil::GetLastPathPart(path));
		}

		if (open)
		{
			// Each call draws one direct subfolder and everything inside it.
			while (next < m_Folders.Size() && EditorPathUtil::IsSubfolder(m_Folders[next].path.CStr(), path))
			{
				next = DrawFolder(world, next, selectedActor, edits);
			}
			DrawActors(world, folder.firstActor, folder.actorCount, selectedActor, edits);
			ImGui::TreePop();
		}
		else
		{
			while (next < m_Folders.Size() && EditorPathUtil::IsSubfolder(m_Folders[next].path.CStr(), path))
			{
				++next;
			}
		}

		ImGui::PopID();
		return next;
	}

	void LevelHierarchyPanel::DrawActors(World& world, uint firstActor, uint actorCount, Entity& selectedActor, PendingEdits& edits)
	{
		for (uint i = firstActor; i < firstActor + actorCount; ++i)
		{
			ActorInstance& actor = world.actorInstances[m_ActorOrder[i]];
			const Entity rootEntity = actor.RootEntity();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen
				| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
			if (rootEntity == selectedActor)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::PushID(static_cast<int>(rootEntity));
			const bool renaming = m_RenamingActor == rootEntity;
			ImGui::TreeNodeEx("##Actor", flags, "%s", renaming ? "" : actor.name.CStr());
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				selectedActor = rootEntity;
			}

			if (m_Editing && ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(c_ActorPayload, &rootEntity, sizeof(Entity));
				ImGui::TextUnformatted(actor.name.CStr());
				ImGui::EndDragDropSource();
			}

			if (m_Editing && ImGui::BeginPopupContextItem("##ActorMenu"))
			{
				if (ImGui::MenuItem("Rename"))
				{
					m_RenamingActor = rootEntity;
					m_RenamingFolder = {};
					m_RenameFocusPending = true;
					strcpy_s(m_RenameBuffer, actor.name.CStr());
				}
				if (ImGui::MenuItem("Delete", "Del"))
				{
					edits.actorToDelete = rootEntity;
				}
				ImGui::EndPopup();
			}

			if (renaming)
			{
				ImGui::SameLine();
				// Actor names are shorter than folder names.
				const EditorWidgets::RenameResult result = EditorWidgets::DrawRenameBox(m_RenameBuffer, ActorName::c_Capacity, m_RenameFocusPending);
				if (result == EditorWidgets::RenameResult::Committed && m_RenameBuffer[0] != '\0')
				{
					// Cut short to fit, the same as names given when spawning.
					m_RenameBuffer[std::min<size_t>(strlen(m_RenameBuffer), c_MaxActorName)] = '\0';
					actor.name = m_RenameBuffer;
					++world.changeCount;
				}
				if (result != EditorWidgets::RenameResult::Editing)
				{
					m_RenamingActor = c_InvalidEntity;
				}
			}

			ImGui::PopID();
		}
	}

	void LevelHierarchyPanel::DrawFolderMenu(const char* folderPath, PendingEdits& edits)
	{
		if (!m_Editing || !ImGui::BeginPopupContextItem("##FolderMenu"))
		{
			return;
		}

		if (ImGui::MenuItem(c_AddActorName))
		{
			OpenActorPicker(folderPath);
		}
		if (ImGui::MenuItem(c_NewFolderName))
		{
			edits.createFolder = true;
			edits.newFolderParent = folderPath;
		}
		if (ImGui::MenuItem("Rename"))
		{
			m_RenamingFolder = folderPath;
			m_RenamingActor = c_InvalidEntity;
			m_RenameFocusPending = true;
			strcpy_s(m_RenameBuffer, EditorPathUtil::GetLastPathPart(folderPath));
		}
		// Like Unreal, deleting a folder keeps what's in it, moving it up a level.
		if (ImGui::MenuItem("Delete"))
		{
			edits.replaceFolder = true;
			edits.folderToReplace = folderPath;
			edits.replacement = GetParentFolder(folderPath);
		}
		ImGui::EndPopup();
	}

	void LevelHierarchyPanel::AcceptDrop(const char* folderPath, PendingEdits& edits)
	{
		if (!m_Editing || !ImGui::BeginDragDropTarget())
		{
			return;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(c_ActorPayload))
		{
			edits.actorToMove = *static_cast<const Entity*>(payload->Data);
			edits.moveTarget = folderPath;
		}
		// A mesh from the Asset Browser is placed in the level, inside this folder.
		else if (const ImGuiPayload* meshPayload = ImGui::AcceptDragDropPayload(AssetBrowserPanel::c_MeshPayload))
		{
			edits.placeMesh = true;
			edits.mesh = *static_cast<const AssetID*>(meshPayload->Data);
			edits.meshFolder = folderPath;
		}
		ImGui::EndDragDropTarget();
	}

	void LevelHierarchyPanel::ApplyEdits(World& world, Entity& selectedActor, const PendingEdits& edits)
	{
		if (edits.actorToDelete != c_InvalidEntity)
		{
			DeleteActor(edits.actorToDelete, selectedActor);
		}

		if (edits.actorToMove != c_InvalidEntity)
		{
			for (ActorInstance& actor : world.actorInstances)
			{
				if (actor.RootEntity() == edits.actorToMove)
				{
					actor.folderPath = edits.moveTarget;
					++world.changeCount;
					break;
				}
			}
		}

		if (edits.replaceFolder)
		{
			ReplaceFolder(world, edits.folderToReplace.CStr(), edits.replacement.CStr());
		}

		if (edits.createFolder)
		{
			CreateFolder(world, edits.newFolderParent.CStr());
		}
	}

	void LevelHierarchyPanel::ReplaceFolder(World& world, const char* oldPath, const char* newPath)
	{
		const size_t oldLength = strlen(oldPath);

		// Returns false, leaving path as it was, when the new path would be too long.
		auto remap = [oldPath, oldLength, newPath](RelativePath& path) -> bool
		{
			if (path == oldPath)
			{
				path = newPath;
				return true;
			}
			if (!EditorPathUtil::IsSubfolder(path.CStr(), oldPath))
			{
				return true;
			}
			RelativePath remapped;
			if (!JoinPath(newPath, path.CStr() + oldLength + 1, remapped))
			{
				return false;
			}
			path = remapped;
			return true;
		};

		bool allFit = true;
		for (ActorInstance& actor : world.actorInstances)
		{
			allFit &= remap(actor.folderPath);
		}
		for (uint i = world.folders.Size(); i-- > 0;)
		{
			allFit &= remap(world.folders[i]);
			// A folder moved to the root is gone.
			if (world.folders[i].Size() == 0)
			{
				world.folders.SwapAndPopBack(i);
			}
		}
		if (!allFit)
		{
			TYR_LOG_WARNING("Some of %s was left in place, as its new path would be too long.", oldPath);
		}
		++world.changeCount;
	}

	void LevelHierarchyPanel::CreateFolder(World& world, const char* parentPath)
	{
		// "New Folder", then "New Folder 2" and so on, until it's not already taken.
		RelativePath path;
		char name[RelativePath::c_Capacity];
		for (uint number = 1;; ++number)
		{
			if (number == 1)
			{
				snprintf(name, sizeof(name), "%s", c_NewFolderName);
			}
			else
			{
				snprintf(name, sizeof(name), "%s %u", c_NewFolderName, number);
			}
			if (!JoinPath(parentPath, name, path))
			{
				TYR_LOG_WARNING("Can't make a folder in %s, the path would be too long.", parentPath);
				return;
			}

			bool taken = false;
			for (const FolderEntry& folder : m_Folders)
			{
				taken |= folder.path == path;
			}
			if (!taken)
			{
				break;
			}
		}

		world.folders.Add(path);
		++world.changeCount;

		// Like Unreal, a new folder starts out being renamed.
		m_RenamingFolder = path;
		m_RenamingActor = c_InvalidEntity;
		m_RenameFocusPending = true;
		strcpy_s(m_RenameBuffer, name);
	}
}
