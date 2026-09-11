/// Copyright (c) 2023 Aidan Clear 

#include "Editor.h"
#include "BuildConfig.h"
#include "AssetSystem/AssetModule.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetRegistry.h"
#include "Window/WindowModule.h"
#include "Window/WindowDesc.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Math/Vector2.h"
#include "World/Camera.h"
#include "AssetSystem/AssetUtil.h"
#include "Importing/MaterialImporter.h"

namespace tyr
{
	Editor::Editor()
	{
		
	}

	Editor::~Editor()
	{
		
	}

	void Editor::Initialize()
	{
		TYR_GET_MODULE(WindowModule, m_WindowModule);
		{
			// Get project related properties from the config later
			WindowDesc desc;
			desc.showFlag = 1;
			desc.name = c_AppName;
			Platform::GetMaxWindowResolution(desc.width, desc.height);
			m_PrimaryWindow = m_WindowModule->MakeWindow(desc);
		}

		AssetModule* assetModule;
		TYR_GET_MODULE(AssetModule, assetModule);
		m_AssetManager = assetModule->GetAssetManager();

		WorldModule* worldModule;
		TYR_GET_MODULE(WorldModule, worldModule);
		m_WorldManager = worldModule->GetWorldManager();

		// Default viewport
		WorldConfig worldParams{};

		m_Camera = MakeURef<Camera>(Vector3(0, 0 ,0), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

		worldParams.camera = m_Camera.get();
		m_LevelEditorWorld = m_WorldManager->AddWorld(worldParams);

		// TEMPORARY CODE FOR TESTING
		if (false)
		{
			PbrMaterialImportDesc desc;
			char materialOutputFolderPath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(materialOutputFolderPath, sizeof(materialOutputFolderPath), "%s/%s", AssetConstants::c_DefaultMaterialFolderName, AssetConstants::c_DefaultMaterialName);
			desc.outputFolderPath = materialOutputFolderPath;
			desc.materialName = AssetConstants::c_DefaultMaterialName;
			desc.albedoSource.path = "C:\\Users\\volca\\Content\\used-stainless-steel\\used-stainless-steel_albedo.png";
			desc.normalSource.path = "C:\\Users\\volca\\Content\\used-stainless-steel\\used-stainless-steel_normal.png";
			desc.heightSource.path = "C:\\Users\\volca\\Content\\used-stainless-steel\\used-stainless-steel_height.png";
			desc.occlusionSource.path = "C:\\Users\\volca\\Content\\used-stainless-steel\\used-stainless-steel_ao.png";
			desc.roughnessMetallicSource.path = "C:\\Users\\volca\\Content\\used-stainless-steel\\used-stainless-steel_roughness.png";

			const char* materialPath = m_AssetManager->GetDefaultMaterialPath();
			char absMaterialPath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absMaterialPath, materialPath);

			const StringView fsPath(absMaterialPath);

			bool loadMaterial = true;
			if (!fs::exists(fsPath))
			{
				loadMaterial = MaterialImporter::Instance().ImportPbrMaterial(desc);
				TYR_ASSERT(loadMaterial);
			}
		}
	}

	void Editor::Update(float deltaTime)
	{
		
	}

	void Editor::Shutdown()
	{
		m_WorldManager->RemoveWorld(m_LevelEditorWorld);
		m_LevelEditorWorld = {};
		m_WorldManager = nullptr;
		m_WindowModule->DestroyWindow(m_PrimaryWindow);
	}

	bool Editor::WantsExit() const
	{
		// TODO: Handle headless case
		return m_WindowModule->IsWindowActive(m_PrimaryWindow);
	}
}