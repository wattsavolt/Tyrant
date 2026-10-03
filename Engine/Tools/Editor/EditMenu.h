#pragma once

namespace tyr
{
	class RendererAPI;

	// Draws the Edit menu and handles its items.
	class EditMenu final
	{
	public:
		EditMenu(RendererAPI& rendererAPI);

		void Draw();

	private:
		void DrawProjectSettingsWindow();

		RendererAPI& m_RendererAPI;
		bool m_ShowProjectSettings = false;
		// Mirrors whatever was last picked in this UI - there's no getter back from the renderer
		// (its own quality state is worker-owned, not safe to read from here), and nothing else
		// calls SetQualityLevel/SetTaaEnabled, so this is never out of sync with reality. Defaults
		// match Renderer's own built-in defaults (Ultra, TAA on).
		int m_QualityLevelIndex = 3;
		bool m_TaaEnabled = true;
	};
}
