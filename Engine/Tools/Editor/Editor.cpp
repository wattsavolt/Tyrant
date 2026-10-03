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
#include "Importing/ModelImporter.h"
#include "ECS/Components.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"
#include "GUI/GUIModule.h"
#include "Input/InputModule.h"
#include "Input/InputManager.h"

namespace tyr
{
	Editor::Editor(GUIModule& guiModule)
		: m_GUIModule(&guiModule)
	{
	}

	Editor::~Editor()
	{
		
	}

	void Editor::Initialize()
	{
		TYR_GET_MODULE(WindowModule, m_WindowModule);

		AssetModule* assetModule;
		TYR_GET_MODULE(AssetModule, assetModule);
		m_AssetManager = assetModule->GetAssetManager();

		WorldModule* worldModule;
		TYR_GET_MODULE(WorldModule, worldModule);
		m_WorldManager = worldModule->GetWorldManager();

		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();

		// Get project related properties from the config later
		WindowDesc desc;
		desc.showFlag = 1;
		desc.name = c_AppName;
		Platform::GetMaxWindowResolution(desc.width, desc.height);

		// Pulled back from the origin along -Z, looking down +Z (c_Forward) toward it - the test
		// cube below is placed at the origin too, and a camera sitting exactly on top of what
		// it's meant to look at ends up with the whole thing behind or inside the near plane.
		m_Camera = MakeURef<Camera>(Vector3(0, 0, -3), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

		// Default viewport
		WorldConfig worldParams{};
		worldParams.camera = m_Camera.get();

		m_LevelEditorWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);
		m_GUIModule->SetPrimaryWindow(m_WindowModule, m_PrimaryWindow);

		InputModule* inputModule;
		TYR_GET_MODULE(InputModule, inputModule);
		inputModule->GetInputManager()->SetWindow(m_WindowModule, m_PrimaryWindow);

		m_EditorViewport = MakeURef<EditorViewport>(*m_RendererAPI);
		m_EditorUI = MakeURef<EditorUI>(*m_GUIModule, *m_RendererAPI);

		// Step 7 test entity: import the test cube mesh if it hasn't been imported yet, then
		// spawn a single entity for it with a transform and mesh component. This is
		// temporary roadmap test code, not permanent editor logic.
		{
			char meshPath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(meshPath, sizeof(meshPath), "Models/Cube/Cube%s", AssetConstants::c_MeshFileExtension);

			AssetID meshAssetID = AssetRegistry::Instance().GetAssetIDSafe(meshPath);
			if (!AssetUtil::IsValidAssetID(meshAssetID))
			{
				const bool importedModel = ModelImporter::Instance().ImportModel(
					"C:\\Users\\volca\\Content\\Cube\\Cube.glb", "Models/Cube", "Cube");
				TYR_ASSERT(importedModel);

				meshAssetID = AssetRegistry::Instance().GetAssetIDSafe(meshPath);
				TYR_ASSERT(AssetUtil::IsValidAssetID(meshAssetID));
			}

			World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
			Entity testEntity = world.entities.CreateEntity();

			ComponentTransform transform;
			transform.local.scale = Vector3::c_One;
			transform.local.rotation = Quaternion::c_Identity;
			transform.local.position = Vector3::c_Zero;
			transform.world = transform.local;
			world.entities.AddComponent<ComponentTransform>(testEntity, transform);

			MeshComponent meshComponent;
			meshComponent.mesh = meshAssetID;
			world.entities.AddComponent<MeshComponent>(testEntity, meshComponent);
			// WorldManager::UpdateWorld creates the actual mesh instance from this component -
			// no need (and would double up the instance) to also call CreateMeshInstance here.
		}

		// Test light: without at least one directional light plus the ambient term set in
		// WorldManager::UpdateWorld, MeshPS.hlsl has literally nothing to shade with and every
		// pixel comes out black regardless of whether the geometry/camera are otherwise correct.
		{
			World& world = m_WorldManager->GetWorld(m_LevelEditorWorld);
			Entity lightEntity = world.entities.CreateEntity();

			DirLightComponent lightComponent;
			// The direction the light travels (light -> scene, matching SpotLightInfo's own
			// direction convention) - see ComputeDirectionalLightEffect's comment.
			lightComponent.direction = Vector3::Normalize(Vector3(-0.4f, -0.8f, 0.4f));
			lightComponent.colour = Vector3::c_One;
			lightComponent.intensity = 3.0f;
			lightComponent.castsShadow = false;
			world.entities.AddComponent<DirLightComponent>(lightEntity, lightComponent);
			// WorldManager::UpdateWorld creates the actual renderer-side light from this
			// component - no need to also call CreateDirectionalLight here.
		}
	}

	void Editor::Update(float deltaTime)
	{
		m_EditorUI->Draw();
		m_EditorViewport->Draw(m_WorldManager->GetWorld(m_LevelEditorWorld).renderViewportHandle);
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