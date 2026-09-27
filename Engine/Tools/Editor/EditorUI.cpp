#include "EditorUI.h"
#include "GUI/GUIModule.h"
#include "imgui.h"

namespace tyr
{
	EditorUI::EditorUI(GUIModule& guiModule)
		: m_GUIModule(guiModule)
	{
	}

	void EditorUI::Draw()
	{
		// TyrantEditor and TyrantEngine are separate DLLs, each with their own statically
		// linked copy of ImGui - each has its own GImGui global, so this has to be set here too,
		// not just where GUIModule created the context.
		ImGui::SetCurrentContext(m_GUIModule.GetImGuiContext());

		if (ImGui::BeginMainMenuBar())
		{
			m_FileMenu.Draw();
			m_EditMenu.Draw();
			ImGui::EndMainMenuBar();
		}
	}
}
