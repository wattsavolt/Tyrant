#pragma once

#include "EditorMacros.h"
#include "EditorLayout.h"
#include "EditorIcons.h"

namespace tyr
{
	enum class PlayState : uint8
	{
		Editing,
		Playing,
		Paused
	};

	// The strip above the viewport with the play, pause and stop buttons.
	class TYR_EDITOR_EXPORT EditorToolbar final
	{
	public:
		EditorToolbar(const EditorIcons& icons);

		// Updates playState when a button is clicked.
		void Draw(const PanelRect& rect, PlayState& playState);

	private:
		bool DrawButton(EditorIcons::Icon icon, bool enabled) const;

		const EditorIcons& m_Icons;
	};
}
