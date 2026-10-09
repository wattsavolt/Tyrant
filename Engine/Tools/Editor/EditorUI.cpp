#include "EditorUI.h"
#include "GUI/GUIModule.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "Utility/PathUtil.h"
#include "imgui.h"
#include <cstdio>

namespace tyr
{
	EditorUI::EditorUI(GUIModule& guiModule, RendererAPI& rendererAPI, AssetManager& assetManager, WorldManager& worldManager, Handle levelWorld, const EditorSettings& settings)
		: m_GUIModule(guiModule)
		, m_FileMenu(settings)
		, m_EditMenu(rendererAPI)
		, m_Icons(assetManager)
		, m_Toolbar(m_Icons)
		, m_HierarchyPanel(worldManager, levelWorld, m_Icons)
		, m_PropertiesPanel(worldManager, assetManager, levelWorld)
		, m_AssetBrowserPanel(assetManager, m_Icons)
	{
		// Like Unreal, a click on a number box starts typing in it, while dragging still changes it.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());
		ImGui::GetIO().ConfigDragClickToInputText = true;
	}

	PanelRect EditorUI::Draw(PlayState& playState, Entity& selectedActor, AssetID openLevel, bool levelDirty, EditorRequests& requests)
	{
		// The editor DLL has its own copy of ImGui, so its context has to be set here too.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());

		const bool editing = playState == PlayState::Editing;

		if (ImGui::BeginMainMenuBar())
		{
			m_FileMenu.Draw(editing, openLevel, levelDirty, requests);
			m_EditMenu.Draw();
			m_ViewMenu.Draw(m_ViewSettings);

			// The open level's name on the right, marked when it has unsaved changes.
			if (AssetUtil::IsValidAssetID(openLevel))
			{
				char name[PathConstants::c_MaxFileNameTotalSize];
				PathUtil::GetFileNameWithoutExtension(AssetRegistry::Instance().GetAssetData(openLevel).filePath.CStr(), name);
				char label[PathConstants::c_MaxFileNameTotalSize + 2];
				snprintf(label, sizeof(label), "%s%s", name, levelDirty ? "*" : "");
				const float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
				ImGui::SameLine(ImGui::GetWindowWidth() - width);
				ImGui::TextUnformatted(label);
			}
			ImGui::EndMainMenuBar();
		}
		m_FileMenu.DrawPopups(requests);

		// Calculated after the menu bar so the layout fits below it.
		const EditorLayout::Panels panels = m_Layout.Calculate();

		m_Toolbar.Draw(panels.toolbar, playState);

		// The left panel's tabs act as its title.
		if (EditorLayout::BeginPanel("Left", panels.left, m_Layout.LeftCollapsed(), false))
		{
			ImGui::SameLine();
			if (ImGui::BeginTabBar("##LeftTabs"))
			{
				if (m_ViewSettings.hierarchyOpen && ImGui::BeginTabItem("Hierarchy", &m_ViewSettings.hierarchyOpen))
				{
					m_HierarchyPanel.Draw(editing, selectedActor, requests);
					ImGui::EndTabItem();
				}
				// Picking an actor type here places one in the level's root folder.
				if (m_ViewSettings.actorsOpen && ImGui::BeginTabItem("Actors", &m_ViewSettings.actorsOpen))
				{
					ImGui::BeginDisabled(!editing);
					Id64 actorType;
					if (m_ActorsTabPicker.Draw(actorType, false, 0.0f))
					{
						requests.placeActor = true;
						requests.actorType = actorType;
						requests.folder = {};
					}
					ImGui::EndDisabled();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		if (EditorLayout::BeginPanel("Properties", panels.right, m_Layout.RightCollapsed(), true))
		{
			m_PropertiesPanel.Draw(editing, selectedActor);
		}
		ImGui::End();

		if (EditorLayout::BeginPanel("Asset Browser", panels.bottom, m_Layout.BottomCollapsed(), true))
		{
			const AssetID levelToOpen = m_AssetBrowserPanel.Draw(openLevel);
			if (editing && AssetUtil::IsValidAssetID(levelToOpen))
			{
				m_FileMenu.OpenLevel(levelToOpen, levelDirty, requests);
			}
		}
		ImGui::End();

		// Like Unreal, Delete removes the selected actor wherever the focus is, unless typing.
		if (editing && selectedActor != c_InvalidEntity && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			m_HierarchyPanel.DeleteActor(selectedActor, selectedActor);
		}

		return panels.viewport;
	}
}
