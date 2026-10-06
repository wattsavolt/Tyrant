#pragma once

#include "EditorMacros.h"

namespace tyr
{
	// Screen-space position and size of one editor panel.
	struct PanelRect
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};

	// Works out where the toolbar, the three collapsible panels and the viewport go each frame.
	// The viewport fills whatever space the panels leave free.
	class TYR_EDITOR_EXPORT EditorLayout final
	{
	public:
		struct Panels
		{
			PanelRect toolbar;
			PanelRect left;
			PanelRect right;
			PanelRect bottom;
			PanelRect viewport;
		};

		Panels Calculate() const;

		// Opens a panel window with a button to collapse or expand it, followed by the name when
		// showTitle is set. The caller must always call ImGui::End() afterwards. Returns true when
		// the panel is expanded.
		static bool BeginPanel(const char* name, const PanelRect& rect, bool& collapsed, bool showTitle);

		bool& LeftCollapsed() { return m_LeftCollapsed; }
		bool& RightCollapsed() { return m_RightCollapsed; }
		bool& BottomCollapsed() { return m_BottomCollapsed; }

	private:
		static constexpr float c_ToolbarHeight = 32.0f;
		static constexpr float c_CollapsedPanelSize = 30.0f;
		static constexpr float c_ExpandedSideWidth = 468.0f;
		static constexpr float c_ExpandedBottomHeight = 396.0f;

		bool m_LeftCollapsed = false;
		bool m_RightCollapsed = false;
		bool m_BottomCollapsed = false;
	};
}
