#include "ViewMenu.h"
#include "imgui.h"

namespace tyr
{
	void ViewMenu::Draw(ViewSettings& settings)
	{
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Hierarchy", nullptr, &settings.hierarchyOpen);
			ImGui::Separator();
			ImGui::MenuItem("Grid", nullptr, &settings.showGrid);
			ImGui::MenuItem("Snap to Grid", nullptr, &settings.snapToGrid);
			ImGui::EndMenu();
		}
	}
}
