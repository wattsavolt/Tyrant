#include "EditorUI.h"
#include "GUI/GUIModule.h"
#include "imgui.h"

namespace tyr
{
	EditorUI::EditorUI(GUIModule& guiModule, RendererAPI& rendererAPI, AssetManager& assetManager)
		: m_GUIModule(guiModule)
		, m_EditMenu(rendererAPI)
		, m_Icons(assetManager)
		, m_Toolbar(m_Icons)
		, m_AssetBrowserPanel(assetManager, m_Icons)
	{
	}

	PanelRect EditorUI::Draw(PlayState& playState)
	{
		// The editor DLL has its own copy of ImGui, so its context has to be set here too.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());

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
					m_HierarchyPanel.Draw();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		if (EditorLayout::BeginPanel("Properties", panels.right, m_Layout.RightCollapsed(), true))
		{
			m_PropertiesPanel.Draw();
		}
		ImGui::End();

		if (EditorLayout::BeginPanel("Asset Browser", panels.bottom, m_Layout.BottomCollapsed(), true))
		{
			m_AssetBrowserPanel.Draw();
		}
		ImGui::End();

		return panels.viewport;
	}
}
