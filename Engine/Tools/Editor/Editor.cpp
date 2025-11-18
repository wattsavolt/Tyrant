/// Copyright (c) 2023 Aidan Clear 

#include "Editor.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Window/Window.h"
#include "Math/Vector2.h"
#include "World/Camera.h"

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
		WorldModule* worldModule;
		TYR_GET_MODULE(WorldModule, worldModule);
		m_WorldManager = worldModule->GetWorldManager();

		// Default viewport
		WorldParams worldParams{};

		m_Camera = MakeURef<Camera>(Vector3(0, 0 ,0), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

		worldParams.camera = m_Camera.get();
		m_LevelEditorWorld = m_WorldManager->AddWorld(worldParams);
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