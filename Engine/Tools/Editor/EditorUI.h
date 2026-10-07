#pragma once

#include "FileMenu.h"
#include "EditMenu.h"
#include "ViewMenu.h"
#include "EditorLayout.h"
#include "EditorToolbar.h"
#include "LevelHierarchyPanel.h"
#include "PropertiesPanel.h"
#include "AssetBrowserPanel.h"
#include "ActorPicker.h"
#include "EditorRequests.h"

namespace tyr
{
	class GUIModule;
	class RendererAPI;
	class AssetManager;
	class WorldManager;

	// Draws the editor's main menu bar, toolbar and side panels each frame.
	class EditorUI final
	{
	public:
		// levelWorld is the world being edited.
		EditorUI(GUIModule& guiModule, RendererAPI& rendererAPI, AssetManager& assetManager, WorldManager& worldManager, Handle levelWorld);

		// Returns where the viewport should be drawn this frame. playState changes when a
		// toolbar button is clicked. selectedActor is the selected actor's root entity, or
		// c_InvalidEntity. requests is filled with anything the panels ask the editor to do.
		PanelRect Draw(PlayState& playState, Entity& selectedActor, EditorRequests& requests);

		// Sends the level editor's or the game's render settings to the renderer.
		void ApplyRenderSettings(bool editing) { m_EditMenu.ApplyRenderSettings(editing); }

		const ViewSettings& GetViewSettings() const { return m_ViewSettings; }
		const EditorIcons& GetIcons() const { return m_Icons; }

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
		// The Actors tab's list, for placing actors in the level's root folder.
		ActorPicker m_ActorsTabPicker;
		ViewSettings m_ViewSettings;
	};
}
