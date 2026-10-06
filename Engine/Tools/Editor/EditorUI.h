#pragma once

#include "FileMenu.h"
#include "EditMenu.h"
#include "ViewMenu.h"
#include "EditorLayout.h"
#include "EditorToolbar.h"
#include "LevelHierarchyPanel.h"
#include "PropertiesPanel.h"
#include "AssetBrowserPanel.h"

namespace tyr
{
	class GUIModule;
	class RendererAPI;
	class AssetManager;

	// Draws the editor's main menu bar, toolbar and side panels each frame.
	class EditorUI final
	{
	public:
		EditorUI(GUIModule& guiModule, RendererAPI& rendererAPI, AssetManager& assetManager);

		// Returns where the viewport should be drawn this frame. playState changes when a
		// toolbar button is clicked.
		PanelRect Draw(PlayState& playState);

		const ViewSettings& GetViewSettings() const { return m_ViewSettings; }

	private:
		GUIModule& m_GUIModule;
		FileMenu m_FileMenu;
		EditMenu m_EditMenu;
		ViewMenu m_ViewMenu;
		EditorLayout m_Layout;
		// Before the panels that use it, so it's constructed first and destroyed last.
		EditorIcons m_Icons;
		EditorToolbar m_Toolbar;
		LevelHierarchyPanel m_HierarchyPanel;
		PropertiesPanel m_PropertiesPanel;
		AssetBrowserPanel m_AssetBrowserPanel;
		ViewSettings m_ViewSettings;
	};
}
