#pragma once

#include "String/Path.h"
#include "AssetSystem/AssetID.h"
#include "EditorRequests.h"

namespace tyr
{
	class EditorSettings;

	// Draws the File menu and handles its items, including the level commands.
	class FileMenu final
	{
	public:
		explicit FileMenu(const EditorSettings& settings);

		// Draws the menu. openLevel is the level being edited and levelDirty whether it has unsaved
		// changes. Level commands are only offered while editing.
		void Draw(bool editing, AssetID openLevel, bool levelDirty, EditorRequests& requests);

		// The unsaved changes and level name popups, drawn outside the menu bar.
		void DrawPopups(EditorRequests& requests);

		// Opens a level, first asking whether to save unsaved changes.
		void OpenLevel(AssetID level, bool levelDirty, EditorRequests& requests);

		// Exits, first asking whether to save unsaved changes.
		void Exit(bool levelDirty, EditorRequests& requests) { BeginAction(LevelAction::Exit, levelDirty, requests); }

	private:
		enum class LevelAction : uint8
		{
			None,
			New,
			Open,
			SaveAs,
			Exit
		};

		void BeginAction(LevelAction action, bool levelDirty, EditorRequests& requests);
		// Moves on once any unsaved changes are dealt with, asking for a name if the action needs one.
		void ContinueAction(EditorRequests& requests);
		void FinishAction(EditorRequests& requests);
		void DrawUnsavedChangesPopup(EditorRequests& requests);
		void DrawLevelNamePopup(EditorRequests& requests);
		// Lists every level, filtered by name, to open one or make it the default.
		void DrawOpenLevelPopup(EditorRequests& requests);

		void Import();
		void DrawImportOptionsWindow();

		const EditorSettings& m_Settings;
		LevelAction m_PendingAction = LevelAction::None;
		AssetID m_PendingLevel;
		bool m_SaveFirst = false;
		bool m_OpenUnsavedChangesPopup = false;
		bool m_OpenLevelNamePopup = false;
		char m_LevelName[PathConstants::c_MaxFileNameTotalSize]{};

		bool m_OpenOpenLevelPopup = false;
		bool m_LevelDirty = false;
		char m_LevelFilter[PathConstants::c_MaxFileNameTotalSize]{};
		AssetID m_SelectedLevel;
		// Read from the editor config when the Open Level popup opens.
		AssetPath m_DefaultLevel;

		bool m_ShowImportOptions = false;
		char m_PendingFilePath[PathConstants::c_MaxPathTotalSize]{};
		char m_PendingModelName[PathConstants::c_MaxFileNameTotalSize]{};
		char m_PendingOutputFolder[PathConstants::c_MaxRelativePathTotalSize]{};

		// Options carried over between imports in the same editor session, so the user doesn't
		// have to re-enter them every time.
		bool m_GenerateStaticMeshLods = true;
		int m_LodCount = 3;
		bool m_ForceLodRegeneration = false;
	};
}
