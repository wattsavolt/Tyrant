#pragma once

#include "EditorMacros.h"
#include "ECS/EntitySystem.h"
#include "String/Path.h"
#include "String/Name.h"
#include "AssetSystem/AssetID.h"
#include "ActorPicker.h"
#include "EditorRequests.h"

namespace tyr
{
	class WorldManager;
	class EditorIcons;
	struct World;

	// The Hierarchy tab, listing the level's actors in folders like Unreal's World Outliner.
	class TYR_EDITOR_EXPORT LevelHierarchyPanel final
	{
	public:
		// Drag and drop payload type carrying an actor's root entity.
		static constexpr const char* c_ActorPayload = "TyrActor";

		LevelHierarchyPanel(WorldManager& worldManager, Handle levelWorld, const EditorIcons& icons);

		// Draws the tab's contents into the current window. selectedActor is the selected actor's
		// root entity, or c_InvalidEntity. Nothing can be changed unless editing. Placing actors
		// and meshes is left to the editor, through requests.
		void Draw(bool editing, Entity& selectedActor, EditorRequests& requests);

		// Removes the actor from the level, clearing the selection if it was selected.
		void DeleteActor(Entity rootEntity, Entity& selectedActor);

	private:
		struct FolderEntry
		{
			RelativePath path;
			// The folder's actors, as a range of m_ActorOrder.
			uint firstActor;
			uint actorCount;
		};

		// Changes made while drawing, applied once drawing is done since they reorder the lists.
		struct PendingEdits
		{
			Entity actorToDelete = c_InvalidEntity;
			Entity actorToMove = c_InvalidEntity;
			RelativePath moveTarget;
			// Paths under folderToReplace move under replacement. Renames and deletes both use this.
			RelativePath folderToReplace;
			RelativePath replacement;
			bool replaceFolder = false;
			// Set to create a folder inside newFolderParent.
			bool createFolder = false;
			RelativePath newFolderParent;
			// A mesh dropped onto a folder, to be placed in the level inside it.
			bool placeMesh = false;
			AssetID mesh;
			RelativePath meshFolder;
		};

		void RebuildIfNeeded(const World& world);
		void Rebuild(const World& world);

		// Draws the folder and everything in it, returning the index of the next folder that
		// isn't inside it.
		uint DrawFolder(World& world, uint folderIndex, Entity& selectedActor, PendingEdits& edits);
		void DrawActors(World& world, uint firstActor, uint actorCount, Entity& selectedActor, PendingEdits& edits);
		void DrawFolderMenu(const char* folderPath, PendingEdits& edits);
		// Takes actors moved onto the last drawn item, and meshes dropped onto it from the Asset Browser.
		void AcceptDrop(const char* folderPath, PendingEdits& edits);
		// The Add Actor popup, which places the picked actor type in a folder.
		void OpenActorPicker(const char* folderPath);
		void DrawActorPicker(EditorRequests& requests);

		void ApplyEdits(World& world, Entity& selectedActor, const PendingEdits& edits);
		// Moves every actor and created folder under oldPath to be under newPath, which may be
		// the root.
		void ReplaceFolder(World& world, const char* oldPath, const char* newPath);
		void CreateFolder(const char* parentPath);

		WorldManager& m_WorldManager;
		Handle m_LevelWorld;
		const EditorIcons& m_Icons;
		// Every folder, sorted so each comes straight after its parent.
		Array<FolderEntry> m_Folders;
		// Indices into the world's actor instances, sorted by folder then name.
		Array<uint> m_ActorOrder;
		// Root level actors come first in m_ActorOrder.
		uint m_RootActorCount = 0;
		// Folders made in this panel, which may not hold any actors yet. Only kept for this
		// session until levels can be saved.
		Array<RelativePath> m_CreatedFolders;
		uint m_EntitiesVersion = ~0u;
		bool m_Dirty = true;
		// Whether this frame's Draw can change anything.
		bool m_Editing = true;

		// The folder or actor being renamed in place, if any.
		RelativePath m_RenamingFolder;
		Entity m_RenamingActor = c_InvalidEntity;
		bool m_RenameFocusPending = false;
		char m_RenameBuffer[RelativePath::c_Capacity] = {};

		ActorPicker m_ActorPicker;
		RelativePath m_ActorPickerFolder;
		bool m_OpenActorPicker = false;
	};
}
