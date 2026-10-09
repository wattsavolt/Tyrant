#include "FileMenu.h"
#include "imgui.h"
#include "Platform/Platform.h"
#include "Utility/PathUtil.h"
#include "Math/Math.h"
#include "Importing/ModelImporter.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "RenderResource/MeshDesc.h"
#include "EditorSettings.h"
#include "EditorWidgets.h"
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		constexpr const char* c_UnsavedChangesPopup = "Unsaved Changes";
		constexpr const char* c_LevelNamePopup = "Level Name";
		constexpr const char* c_OpenLevelPopup = "Open Level";
		constexpr float c_OpenLevelRows = 12.0f;
		constexpr float c_OpenLevelWidthInFontSizes = 24.0f;

		// Keeps the level's path short enough to be remembered in the editor config.
		constexpr size_t c_MaxLevelNameLength = 40;

		bool HasExtension(const char* path, const char* extension)
		{
			const char* pathExtension = strrchr(path, '.');
			return pathExtension && strcmp(pathExtension, extension) == 0;
		}

		// Letters, numbers, spaces, dashes and underscores, so it's always a valid file name.
		bool IsValidLevelName(const char* name)
		{
			const size_t length = strlen(name);
			if (length == 0 || length > c_MaxLevelNameLength || name[0] == ' ')
			{
				return false;
			}
			for (size_t i = 0; i < length; ++i)
			{
				const unsigned char c = static_cast<unsigned char>(name[i]);
				if (!isalnum(c) && c != ' ' && c != '_' && c != '-')
				{
					return false;
				}
			}
			return true;
		}

		bool LevelExists(const char* name)
		{
			char path[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(path, sizeof(path), "%s/%s%s", AssetConstants::c_LevelFolderName, name, AssetConstants::c_LevelFileExtension);
			return AssetUtil::IsValidAssetID(AssetRegistry::Instance().GetAssetID(path));
		}
	}

	FileMenu::FileMenu(const EditorSettings& settings)
		: m_Settings(settings)
	{
	}

	void FileMenu::Draw(bool editing, AssetID openLevel, bool levelDirty, EditorRequests& requests)
	{
		m_LevelDirty = levelDirty;
		if (ImGui::BeginMenu("File"))
		{
			ImGui::BeginDisabled(!editing);
			if (ImGui::MenuItem("New Level..."))
			{
				BeginAction(LevelAction::New, levelDirty, requests);
			}
			if (ImGui::MenuItem("Open Level...", "Ctrl+O"))
			{
				m_OpenOpenLevelPopup = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Save Level", "Ctrl+S"))
			{
				requests.saveLevel = true;
			}
			if (ImGui::MenuItem("Save Level As..."))
			{
				BeginAction(LevelAction::SaveAs, false, requests);
			}
			if (ImGui::MenuItem("Save All", "Ctrl+Shift+S"))
			{
				requests.saveAll = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Set Current Level as Default"))
			{
				requests.setDefaultLevel = true;
				requests.level = openLevel;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Import"))
			{
				Import();
			}
			ImGui::EndDisabled();
			ImGui::Separator();
			if (ImGui::MenuItem("Exit"))
			{
				BeginAction(LevelAction::Exit, levelDirty && editing, requests);
			}
			ImGui::EndMenu();
		}

		if (editing && !ImGui::GetIO().WantTextInput)
		{
			if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
			{
				requests.saveAll = true;
			}
			else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
			{
				requests.saveLevel = true;
			}
			if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
			{
				m_OpenOpenLevelPopup = true;
			}
		}

		if (m_ShowImportOptions)
		{
			DrawImportOptionsWindow();
		}
	}

	void FileMenu::DrawOpenLevelPopup(EditorRequests& requests)
	{
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * c_OpenLevelWidthInFontSizes, 0.0f), ImGuiCond_Appearing);
		if (!ImGui::BeginPopupModal(c_OpenLevelPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			return;
		}

		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Filter", "Search levels", m_LevelFilter, sizeof(m_LevelFilter));

		bool openSelected = false;
		const ImVec2 listSize(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * c_OpenLevelRows);
		if (ImGui::BeginListBox("##Levels", listSize))
		{
			for (const std::pair<const AssetID&, const RegAssetData&> asset : AssetRegistry::Instance().GetAssets())
			{
				const char* path = asset.second.filePath.CStr();
				if (!HasExtension(path, AssetConstants::c_LevelFileExtension))
				{
					continue;
				}

				char name[PathConstants::c_MaxFileNameTotalSize];
				PathUtil::GetFileNameWithoutExtension(path, name);
				if (!EditorWidgets::ContainsIgnoreCase(name, m_LevelFilter))
				{
					continue;
				}

				char label[PathConstants::c_MaxFileNameTotalSize + 16];
				snprintf(label, sizeof(label), "%s%s", name, m_DefaultLevel == path ? "  (Default)" : "");
				ImGui::PushID(path);
				if (ImGui::Selectable(label, asset.first == m_SelectedLevel, ImGuiSelectableFlags_AllowDoubleClick))
				{
					m_SelectedLevel = asset.first;
					openSelected = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
				}
				ImGui::SetItemTooltip("%s", path);
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		const bool hasSelection = AssetUtil::IsValidAssetID(m_SelectedLevel) && AssetRegistry::Instance().GetAssets().Contains(m_SelectedLevel);
		ImGui::BeginDisabled(!hasSelection);
		openSelected |= ImGui::Button("Open");
		ImGui::SameLine();
		if (ImGui::Button("Set as Default"))
		{
			requests.setDefaultLevel = true;
			requests.level = m_SelectedLevel;
			m_DefaultLevel = AssetRegistry::Instance().GetAssetData(m_SelectedLevel).filePath;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

		if (openSelected && hasSelection)
		{
			ImGui::CloseCurrentPopup();
			OpenLevel(m_SelectedLevel, m_LevelDirty, requests);
		}
		else if (cancel)
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void FileMenu::OpenLevel(AssetID level, bool levelDirty, EditorRequests& requests)
	{
		m_PendingLevel = level;
		BeginAction(LevelAction::Open, levelDirty, requests);
	}

	void FileMenu::BeginAction(LevelAction action, bool levelDirty, EditorRequests& requests)
	{
		m_PendingAction = action;
		m_SaveFirst = false;
		if (levelDirty)
		{
			m_OpenUnsavedChangesPopup = true;
			return;
		}
		ContinueAction(requests);
	}

	void FileMenu::ContinueAction(EditorRequests& requests)
	{
		if (m_PendingAction == LevelAction::New || m_PendingAction == LevelAction::SaveAs)
		{
			m_LevelName[0] = '\0';
			m_OpenLevelNamePopup = true;
			return;
		}
		FinishAction(requests);
	}

	void FileMenu::FinishAction(EditorRequests& requests)
	{
		requests.saveLevel |= m_SaveFirst;
		switch (m_PendingAction)
		{
		case LevelAction::New:
			requests.newLevel = true;
			requests.levelName = m_LevelName;
			break;
		case LevelAction::Open:
			requests.openLevel = true;
			requests.level = m_PendingLevel;
			break;
		case LevelAction::SaveAs:
			requests.saveLevelAs = true;
			requests.levelName = m_LevelName;
			break;
		case LevelAction::Exit:
			requests.exit = true;
			break;
		case LevelAction::None:
			break;
		}
		m_PendingAction = LevelAction::None;
		m_SaveFirst = false;
	}

	void FileMenu::DrawPopups(EditorRequests& requests)
	{
		if (m_OpenUnsavedChangesPopup)
		{
			ImGui::OpenPopup(c_UnsavedChangesPopup);
			m_OpenUnsavedChangesPopup = false;
		}
		DrawUnsavedChangesPopup(requests);

		if (m_OpenLevelNamePopup)
		{
			ImGui::OpenPopup(c_LevelNamePopup);
			m_OpenLevelNamePopup = false;
		}
		DrawLevelNamePopup(requests);

		if (m_OpenOpenLevelPopup)
		{
			ImGui::OpenPopup(c_OpenLevelPopup);
			m_OpenOpenLevelPopup = false;
			m_LevelFilter[0] = '\0';
			m_SelectedLevel = {};
			if (!m_Settings.GetDefaultLevel(m_DefaultLevel))
			{
				m_DefaultLevel = {};
			}
		}
		DrawOpenLevelPopup(requests);
	}

	void FileMenu::DrawUnsavedChangesPopup(EditorRequests& requests)
	{
		if (!ImGui::BeginPopupModal(c_UnsavedChangesPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			return;
		}

		ImGui::TextUnformatted("The level has unsaved changes. Save them first?");
		ImGui::Spacing();
		if (ImGui::Button("Save"))
		{
			m_SaveFirst = true;
			ImGui::CloseCurrentPopup();
			ContinueAction(requests);
		}
		ImGui::SameLine();
		if (ImGui::Button("Don't Save"))
		{
			ImGui::CloseCurrentPopup();
			ContinueAction(requests);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			m_PendingAction = LevelAction::None;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void FileMenu::DrawLevelNamePopup(EditorRequests& requests)
	{
		if (!ImGui::BeginPopupModal(c_LevelNamePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			return;
		}

		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		const bool entered = ImGui::InputText("##LevelName", m_LevelName, c_MaxLevelNameLength + 1, ImGuiInputTextFlags_EnterReturnsTrue);

		const bool validName = IsValidLevelName(m_LevelName);
		const bool exists = validName && LevelExists(m_LevelName);
		if (m_LevelName[0] != '\0' && !validName)
		{
			ImGui::TextDisabled("Use letters, numbers, spaces, dashes and underscores.");
		}
		else if (exists)
		{
			ImGui::TextDisabled("A level with this name already exists.");
		}

		ImGui::BeginDisabled(!validName || exists);
		if (ImGui::Button("OK") || (entered && validName && !exists))
		{
			ImGui::CloseCurrentPopup();
			FinishAction(requests);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			m_PendingAction = LevelAction::None;
			m_SaveFirst = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void FileMenu::Import()
	{
		const bool picked = Platform::ShowOpenFileDialog(m_PendingFilePath, sizeof(m_PendingFilePath),
			"glTF Files\0*.gltf;*.glb\0", "Import Model");
		if (!picked)
		{
			return;
		}

		PathUtil::GetFileNameWithoutExtension(m_PendingFilePath, m_PendingModelName);
		snprintf(m_PendingOutputFolder, sizeof(m_PendingOutputFolder), "Models/%s", m_PendingModelName);

		m_ShowImportOptions = true;
	}

	void FileMenu::DrawImportOptionsWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(380, 180), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Import Model", &m_ShowImportOptions))
		{
			ImGui::Text("%s", m_PendingModelName);
			ImGui::Separator();

			ImGui::Checkbox("Generate Static Mesh LODs", &m_GenerateStaticMeshLods);

			ImGui::BeginDisabled(!m_GenerateStaticMeshLods);
			ImGui::InputInt("LOD Count", &m_LodCount);
			m_LodCount = Math::Clamp(m_LodCount, 1, (int)MeshConstants::c_MaxLods - 1);
			ImGui::EndDisabled();

			ImGui::Checkbox("Force LOD Regeneration", &m_ForceLodRegeneration);

			ImGui::Separator();
			if (ImGui::Button("Import"))
			{
				ModelImportOptions options;
				options.generateLods = m_GenerateStaticMeshLods;
				options.lodCount = (uint)m_LodCount;
				options.forceLodGeneration = m_ForceLodRegeneration;

				ModelImporter::Instance().ImportModel(m_PendingFilePath, m_PendingOutputFolder, m_PendingModelName, options);
				// Saved straight away so the import is never lost if the editor doesn't close cleanly.
				AssetRegistry::Instance().Save();
				m_ShowImportOptions = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				m_ShowImportOptions = false;
			}
		}
		ImGui::End();
	}
}
