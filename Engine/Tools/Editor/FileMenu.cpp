#include "FileMenu.h"
#include "imgui.h"
#include "Platform/Platform.h"
#include "Utility/PathUtil.h"
#include "Math/Math.h"
#include "Importing/ModelImporter.h"
#include "RenderResource/MeshDesc.h"

namespace tyr
{
	void FileMenu::Draw()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Import"))
			{
				Import();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit"))
			{
				Platform::Exit(true);
			}
			ImGui::EndMenu();
		}

		if (m_ShowImportOptions)
		{
			DrawImportOptionsWindow();
		}
	}

	void FileMenu::Import()
	{
		const bool picked = Platform::ShowOpenFileDialog(m_PendingFilePath, sizeof(m_PendingFilePath),
			"glTF Files\0*.gltf;*.glb\0", "Import Model");
		if (!picked)
		{
			return;
		}

		PathUtil::GetFileNameWithoutExtension(m_PendingFilePath, m_PendingModelName);
		snprintf(m_PendingOutputFolder, sizeof(m_PendingOutputFolder), "Models/%s", m_PendingModelName);

		m_ShowImportOptions = true;
	}

	void FileMenu::DrawImportOptionsWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(380, 180), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Import Model", &m_ShowImportOptions))
		{
			ImGui::Text("%s", m_PendingModelName);
			ImGui::Separator();

			ImGui::Checkbox("Generate Static Mesh LODs", &m_GenerateStaticMeshLods);

			ImGui::BeginDisabled(!m_GenerateStaticMeshLods);
			ImGui::InputInt("LOD Count", &m_LodCount);
			m_LodCount = Math::Clamp(m_LodCount, 1, (int)MeshConstants::c_MaxLods - 1);
			ImGui::EndDisabled();

			ImGui::Checkbox("Force LOD Regeneration", &m_ForceLodRegeneration);

			ImGui::Separator();
			if (ImGui::Button("Import"))
			{
				ModelImportOptions options;
				options.generateLods = m_GenerateStaticMeshLods;
				options.lodCount = (uint)m_LodCount;
				options.forceLodGeneration = m_ForceLodRegeneration;

				ModelImporter::Instance().ImportModel(m_PendingFilePath, m_PendingOutputFolder, m_PendingModelName, options);
				m_ShowImportOptions = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				m_ShowImportOptions = false;
			}
		}
		ImGui::End();
	}
}
