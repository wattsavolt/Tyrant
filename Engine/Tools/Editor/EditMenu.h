#pragma once

#include "Rendering/RenderQualitySettings.h"

namespace tyr
{
	class RendererAPI;

	// Draws the Edit menu and handles its items.
	class EditMenu final
	{
	public:
		EditMenu(RendererAPI& rendererAPI);

		void Draw();

		// Sends the level editor's or the game's render settings to the renderer.
		void ApplyRenderSettings(bool editing);

	private:
		struct RenderSettings
		{
			QualityLevel quality;
			bool taaEnabled;
		};

		void DrawProjectSettingsWindow();
		// Returns true when the settings were changed.
		static bool DrawRenderSettings(const char* label, RenderSettings& settings);

		RendererAPI& m_RendererAPI;
		bool m_ShowProjectSettings = false;
		// The level editor renders more cheaply than the game, which defaults to the best quality.
		RenderSettings m_LevelEditorSettings = { QualityLevel::Medium, false };
		RenderSettings m_GameSettings = { QualityLevel::Ultra, true };
		bool m_Editing = true;
	};
}
