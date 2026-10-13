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

	void EditMenu::Draw(bool editing, bool canUndo, bool canRedo, EditorRequests& requests)
	{
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editing && canUndo))
			{
				requests.undo = true;
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editing && canRedo))
			{
				requests.redo = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Level Settings"))
			{
				m_ShowLevelSettings = true;
			}
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

		// Text boxes keep their own undo while typing in them.
		if (editing && !ImGui::GetIO().WantTextInput)
		{
			if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
			{
				requests.undo = true;
			}
			else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
			{
				requests.redo = true;
			}
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
		ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
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
			DrawDisplaySettings();
		}
		ImGui::End();
	}

	void EditMenu::DrawDisplaySettings()
	{
		ImGui::SeparatorText("Display");

		DisplaySettings settings = m_RendererAPI.GetDisplaySettings();
		bool changed = ImGui::Checkbox("HDR", &settings.hdrEnabled);
		ImGui::SetItemTooltip("Used whenever the display is in HDR mode.");
		changed |= ImGui::DragFloat("Paper White (nits)", &settings.paperWhiteNits, 1.0f, 80.0f, 1000.0f, "%.0f");
		ImGui::SetItemTooltip("How bright white in the scene is in HDR.");
		changed |= ImGui::DragFloat("UI Brightness (nits)", &settings.uiNits, 1.0f, 80.0f, 1000.0f, "%.0f");
		changed |= ImGui::DragFloat("Peak Brightness (nits)", &settings.peakNits, 10.0f, 0.0f, 10000.0f, settings.peakNits > 0.0f ? "%.0f" : "From display");
		ImGui::SetItemTooltip("The brightest the image gets in HDR. Zero uses what the display reports.");
		if (changed)
		{
			m_RendererAPI.SetDisplaySettings(settings);
		}

		DisplayHDRInfo info;
		DisplayOutput output;
		if (m_RendererAPI.GetActiveDisplay(info, output))
		{
			ImGui::Text("Output: %s", output == DisplayOutput::HDR10 ? "HDR10" : "SDR");
			ImGui::Text("Display: HDR %s, %.0f nits peak, %.0f nits full screen", info.osHdrEnabled ? "on" : "off", info.maxLuminance, info.maxFullFrameLuminance);
		}
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
