#include "EditorToolbar.h"
#include "imgui.h"

namespace tyr
{
	namespace
	{
		constexpr float c_IconSize = 18.0f;
		constexpr uint c_ButtonCount = 3;
	}

	EditorToolbar::EditorToolbar(const EditorIcons& icons)
		: m_Icons(icons)
	{
	}

	void EditorToolbar::Draw(const PanelRect& rect, PlayState& playState)
	{
		ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
		ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height));
		constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoScrollbar;

		ImGui::Begin("##Toolbar", nullptr, flags);

		// Enabled states come from the state at the start of the frame so a click doesn't change
		// the buttons halfway through drawing them.
		const PlayState currentState = playState;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonWidth = c_IconSize + style.FramePadding.x * 2.0f;
		const float buttonHeight = c_IconSize + style.FramePadding.y * 2.0f;
		const float groupWidth = buttonWidth * c_ButtonCount + style.ItemSpacing.x * (c_ButtonCount - 1);
		ImGui::SetCursorPos(ImVec2((rect.width - groupWidth) * 0.5f, (rect.height - buttonHeight) * 0.5f));

		if (DrawButton(EditorIcons::Play, currentState != PlayState::Playing))
		{
			playState = PlayState::Playing;
		}

		ImGui::SameLine();
		if (DrawButton(EditorIcons::Pause, currentState == PlayState::Playing))
		{
			playState = PlayState::Paused;
		}

		ImGui::SameLine();
		if (DrawButton(EditorIcons::Stop, currentState != PlayState::Editing))
		{
			playState = PlayState::Editing;
		}

		ImGui::End();
	}

	bool EditorToolbar::DrawButton(EditorIcons::Icon icon, bool enabled) const
	{
		// The icon's name doubles as its tooltip.
		const char* label = EditorIcons::GetName(icon);
		ImGui::BeginDisabled(!enabled);

		bool clicked;
		const ImTextureID textureID = m_Icons.GetImTextureID(icon);
		if (textureID)
		{
			clicked = ImGui::ImageButton(label, textureID, ImVec2(c_IconSize, c_IconSize));
		}
		else
		{
			// Same size as the icon button, shown until the icon has loaded.
			ImGui::PushID(label);
			const ImGuiStyle& style = ImGui::GetStyle();
			clicked = ImGui::Button("##Icon", ImVec2(c_IconSize + style.FramePadding.x * 2.0f, c_IconSize + style.FramePadding.y * 2.0f));
			ImGui::PopID();
		}
		ImGui::SetItemTooltip("%s", label);

		ImGui::EndDisabled();
		return clicked;
	}
}
