#pragma once

#include "FileMenu.h"
#include "EditMenu.h"

namespace tyr
{
	class GUIModule;

	// Draws the editor's main menu bar each frame.
	class EditorUI final
	{
	public:
		EditorUI(GUIModule& guiModule);

		void Draw();

	private:
		GUIModule& m_GUIModule;
		FileMenu m_FileMenu;
		EditMenu m_EditMenu;
	};
}
