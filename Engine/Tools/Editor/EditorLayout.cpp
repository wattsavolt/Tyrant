#include "EditorLayout.h"
#include "imgui.h"

namespace tyr
{
	EditorLayout::Panels EditorLayout::Calculate() const
	{
		// Work area already excludes the main menu bar.
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		const ImVec2 contentPos = mainViewport->WorkPos;
		const ImVec2 contentSize = mainViewport->WorkSize;

		const float leftWidth = m_LeftCollapsed ? c_CollapsedPanelSize : c_ExpandedSideWidth;
		const float rightWidth = m_RightCollapsed ? c_CollapsedPanelSize : c_ExpandedSideWidth;
		const float bottomHeight = m_BottomCollapsed ? c_CollapsedPanelSize : c_ExpandedBottomHeight;

		const float middleTop = contentPos.y + c_ToolbarHeight;
		const float middleHeight = contentSize.y - c_ToolbarHeight - bottomHeight;

		Panels panels;
		panels.toolbar = PanelRect{ contentPos.x, contentPos.y, contentSize.x, c_ToolbarHeight };
		panels.left = PanelRect{ contentPos.x, middleTop, leftWidth, middleHeight };
		panels.right = PanelRect{ contentPos.x + contentSize.x - rightWidth, middleTop, rightWidth, middleHeight };
		// The bottom panel spans the full width, under the left and right panels.
		panels.bottom = PanelRect{ contentPos.x, contentPos.y + contentSize.y - bottomHeight, contentSize.x, bottomHeight };
		panels.viewport = PanelRect{ contentPos.x + leftWidth, middleTop, contentSize.x - leftWidth - rightWidth, middleHeight };
		return panels;
	}

	bool EditorLayout::BeginPanel(const char* name, const PanelRect& rect, bool& collapsed, bool showTitle)
	{
		ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
		ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height));
		// No title bar so a collapsed panel still has room for its button.
		constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

		ImGui::Begin(name, nullptr, flags);

		if (ImGui::Button(collapsed ? "+" : "-"))
		{
			collapsed = !collapsed;
		}

		if (!collapsed && showTitle)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(name);
			ImGui::Separator();
		}

		return !collapsed;
	}
}
