#include "LevelSettingsWindow.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Reflection/TypeRegistry.h"
#include "imgui.h"

namespace tyr
{
	LevelSettingsWindow::LevelSettingsWindow(WorldManager& worldManager, Handle levelWorld)
		: m_WorldManager(worldManager)
		, m_LevelWorld(levelWorld)
	{
	}

	void LevelSettingsWindow::Draw(bool editing, bool& open)
	{
		if (!open)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(440.0f, 420.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Level Settings", &open))
		{
			const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(GetTypeID<LevelSettings>());
			LevelSettings settings = m_WorldManager.GetWorld(m_LevelWorld).settings;
			const LevelSettings defaults;

			bool changed = false;
			ImGui::BeginDisabled(!editing || !typeInfo);
			constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
			if (typeInfo && ImGui::BeginTable("##LevelSettings", 2, tableFlags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.45f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
				changed = m_Reflector.ReflectObject(&settings, *typeInfo, &defaults);
				ImGui::EndTable();
			}
			ImGui::EndDisabled();

			if (changed && editing)
			{
				m_WorldManager.SetLevelSettings(m_LevelWorld, settings);
			}
		}
		ImGui::End();
	}
}
