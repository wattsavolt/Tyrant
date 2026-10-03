#include "EditMenu.h"
#include "Rendering/RendererAPI.h"
#include "Rendering/RenderQualitySettings.h"
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

	void EditMenu::DrawProjectSettingsWindow()
	{
		// Runtime-only for now, as discussed - this doesn't persist to a project settings file
		// yet, so whatever's picked here reverts to the defaults above next time the editor opens.
		ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Project Settings - Rendering", &m_ShowProjectSettings))
		{
			if (ImGui::Combo("Quality Level", &m_QualityLevelIndex, c_QualityLevelNames, (int)(sizeof(c_QualityLevelNames) / sizeof(c_QualityLevelNames[0]))))
			{
				m_RendererAPI.SetQualityLevel((QualityLevel)m_QualityLevelIndex);
			}

			// Kept independent of the quality combo above on purpose - lets TAA be ruled in or
			// out on its own when tracking down a visual issue, without also changing ray/light
			// counts.
			if (ImGui::Checkbox("Temporal Anti-Aliasing (TAA)", &m_TaaEnabled))
			{
				m_RendererAPI.SetTaaEnabled(m_TaaEnabled);
			}
		}
		ImGui::End();
	}
}
