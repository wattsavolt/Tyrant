/// Copyright (c) 2023 Aidan Clear 

#include "Editor.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Window/Window.h"
#include "Math/Vector2.h"
#include "World/Camera.h"

#include "AssetSystem/MaterialAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "Importing/MaterialImporter.h"

namespace tyr
{
	Editor::Editor()
		: m_WorldManager(nullptr)
		, m_LevelEditorWorld(nullptr)
	{
		
	}

	Editor::~Editor()
	{
		
	}

	void Editor::Initialize()
	{
		WorldModule* worldModule;
		TYR_GET_MODULE(WorldModule, worldModule);
		m_WorldManager = worldModule->GetWorldManager();

		// Default viewport
		WorldParams worldParams{};

		m_Camera = MakeURef<Camera>(Vector3(0, 0 ,0), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

		worldParams.camera = m_Camera.get();
		m_LevelEditorWorld = m_WorldManager->AddWorld(worldParams);

		// TEMPORARY CODE FOR TESTING
		{
			PbrMaterialImportDesc desc;
			desc.outputFolderPath = "Materials/used-stainless_steel";
			desc.materialName = "used-stainless_steel";
			desc.albedoPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_albedo.png";
			desc.normalPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_normal.png";
			desc.heightPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_height.png";
			desc.ambientOcclusionPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_ao.png";
			desc.roughnessPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_roughness.png";
			desc.metallicPath = "C:\\Users\\\\Content\\used-stainless-steel\\used-stainless-steel_metallic.png";

			char materialPath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(materialPath, sizeof(materialPath), "%s/%s%s", desc.outputFolderPath, desc.materialName, c_MaterialFileExtension);

			char absMaterialPath[TYR_MAX_PATH_TOTAL_SIZE];
			AssetUtil::CreateFullPath(absMaterialPath, materialPath);

			const StringView fsPath(absMaterialPath);

			bool loadMaterial = true;
			if (!fs::exists(fsPath))
			{
				loadMaterial = MaterialImporter::Instance().ImportPbrMaterial(desc);
				TYR_ASSERT(loadMaterial);
			}
			MaterialAssetFile material;
			if (loadMaterial)
			{
				AssetUtil::LoadAsset<MaterialAssetFile>(materialPath, material);
			}
		}
	}

	void Editor::Update(float deltaTime)
	{
		
	}

	void Editor::Shutdown()
	{
		m_WorldManager->RemoveWorld(m_LevelEditorWorld);
		m_LevelEditorWorld = nullptr;
		m_WorldManager = nullptr;
	}
}