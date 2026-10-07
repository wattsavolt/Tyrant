#include "EditMenu.h"
#include "Rendering/RendererAPI.h"
#include "imgui.h"

namespace tyr
{
	namespace
	{
		const char* c_QualityLevelNames[] = { "Low", "Medium", "High", "Ultra" };
	}

	EditMenu::EditMenu(RendererAPI& rendererAPI)
		: m_RendererAPI(rendererAPI)
	{
	}

	void EditMenu::Draw()
	{
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::BeginMenu("Project Settings"))
			{
				if (ImGui::MenuItem("Rendering"))
				{
					m_ShowProjectSettings = true;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (m_ShowProjectSettings)
		{
			DrawProjectSettingsWindow();
		}
	}

	void EditMenu::ApplyRenderSettings(bool editing)
	{
		m_Editing = editing;
		const RenderSettings& settings = editing ? m_LevelEditorSettings : m_GameSettings;
		m_RendererAPI.SetQualityLevel(settings.quality);
		m_RendererAPI.SetTaaEnabled(settings.taaEnabled);
	}

	void EditMenu::DrawProjectSettingsWindow()
	{
		// Runtime-only for now - this doesn't persist to a project settings file yet, so these
		// revert to their defaults next time the editor opens.
		ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Project Settings - Rendering", &m_ShowProjectSettings))
		{
			// Only the settings for what's on screen now are sent straight away.
			if (DrawRenderSettings("Level Editor", m_LevelEditorSettings) && m_Editing)
			{
				ApplyRenderSettings(true);
			}
			if (DrawRenderSettings("Game", m_GameSettings) && !m_Editing)
			{
				ApplyRenderSettings(false);
			}
		}
		ImGui::End();
	}

	bool EditMenu::DrawRenderSettings(const char* label, RenderSettings& settings)
	{
		ImGui::SeparatorText(label);
		ImGui::PushID(label);

		int qualityIndex = static_cast<int>(settings.quality);
		bool changed = false;
		if (ImGui::Combo("Quality Level", &qualityIndex, c_QualityLevelNames, static_cast<int>(sizeof(c_QualityLevelNames) / sizeof(c_QualityLevelNames[0]))))
		{
			settings.quality = static_cast<QualityLevel>(qualityIndex);
			changed = true;
		}
		// Separate from the quality level so TAA can be ruled in or out on its own.
		changed |= ImGui::Checkbox("Temporal Anti-Aliasing (TAA)", &settings.taaEnabled);

		ImGui::PopID();
		return changed;
	}
}
