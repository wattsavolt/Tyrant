#pragma once

#include "Rendering/RenderQualitySettings.h"
#include "EditorRequests.h"

namespace tyr
{
	class RendererAPI;

	// Draws the Edit menu and handles its items.
	class EditMenu final
	{
	public:
		EditMenu(RendererAPI& rendererAPI);

		// Undo and redo are only offered while editing the level.
		void Draw(bool editing, bool canUndo, bool canRedo, EditorRequests& requests);

		// Sends the level editor's or the game's render settings to the renderer.
		void ApplyRenderSettings(bool editing);

		// Whether the Level Settings window is open.
		bool& LevelSettingsOpen() { return m_ShowLevelSettings; }

	private:
		struct RenderSettings
		{
			QualityLevel quality;
			bool taaEnabled;
		};

		void DrawProjectSettingsWindow();
		// Returns true when the settings were changed.
		static bool DrawRenderSettings(const char* label, RenderSettings& settings);
		// HDR and brightness, shared by the editor and the game, and what the display can do.
		void DrawDisplaySettings();

		RendererAPI& m_RendererAPI;
		bool m_ShowProjectSettings = false;
		bool m_ShowLevelSettings = false;
		// The level editor renders more cheaply than the game, which defaults to the best quality.
		RenderSettings m_LevelEditorSettings = { QualityLevel::Medium, false };
		RenderSettings m_GameSettings = { QualityLevel::Ultra, true };
		bool m_Editing = true;
	};
}
