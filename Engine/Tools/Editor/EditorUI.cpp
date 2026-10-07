#include "EditorUI.h"
#include "GUI/GUIModule.h"
#include "imgui.h"

namespace tyr
{
	EditorUI::EditorUI(GUIModule& guiModule, RendererAPI& rendererAPI, AssetManager& assetManager, WorldManager& worldManager, Handle levelWorld)
		: m_GUIModule(guiModule)
		, m_EditMenu(rendererAPI)
		, m_Icons(assetManager)
		, m_Toolbar(m_Icons)
		, m_HierarchyPanel(worldManager, levelWorld, m_Icons)
		, m_PropertiesPanel(worldManager, levelWorld)
		, m_AssetBrowserPanel(assetManager, m_Icons)
	{
		// Like Unreal, a click on a number box starts typing in it, while dragging still changes it.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());
		ImGui::GetIO().ConfigDragClickToInputText = true;
	}

	PanelRect EditorUI::Draw(PlayState& playState, Entity& selectedActor, EditorRequests& requests)
	{
		// The editor DLL has its own copy of ImGui, so its context has to be set here too.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());

		const bool editing = playState == PlayState::Editing;

		if (ImGui::BeginMainMenuBar())
		{
			m_FileMenu.Draw();
			m_EditMenu.Draw();
			m_ViewMenu.Draw(m_ViewSettings);
			ImGui::EndMainMenuBar();
		}

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
			m_AssetBrowserPanel.Draw();
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
