#include "FileMenu.h"
#include "imgui.h"
#include "Platform/Platform.h"
#include "Utility/PathUtil.h"
#include "String/Path.h"
#include "Importing/ModelImporter.h"

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
	}

	void FileMenu::Import()
	{
		char filePath[PathConstants::c_MaxPathTotalSize];
		const bool picked = Platform::ShowOpenFileDialog(filePath, sizeof(filePath),
			"glTF Files\0*.gltf;*.glb\0", "Import Model");
		if (!picked)
		{
			return;
		}

		char modelName[PathConstants::c_MaxFileNameTotalSize];
		PathUtil::GetFileNameWithoutExtension(filePath, modelName);

		char outputFolder[PathConstants::c_MaxRelativePathTotalSize];
		snprintf(outputFolder, sizeof(outputFolder), "Models/%s", modelName);

		ModelImporter::Instance().ImportModel(filePath, outputFolder, modelName);
	}
}
