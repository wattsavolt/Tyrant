#include "EditMenu.h"
#include "imgui.h"

namespace tyr
{
	void EditMenu::Draw()
	{
		// No options yet - placeholder until there's undo/redo or preferences to put here.
		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::EndMenu();
		}
	}
}
