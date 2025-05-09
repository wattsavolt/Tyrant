/// Copyright (c) 2023 Aidan Clear 

#include "Editor.h"
#include "Engine.h"
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

	void Editor::Start()
	{
		AppBase::Start();

		const WindowProperties& windowProperties = m_Engine->GetWindowProperties();
		// Default viewport
		WorldParams worldParams{};

		const uint camWidth = m_Engine->GetSurfaceWidth();
		const uint camHeight = m_Engine->GetSurfaceHeight();
		m_Camera = MakeURef<Camera>(Vector3(0, 0 ,0), Vector3::c_Up, Vector3::c_Forward, 90,
			camWidth, camHeight, 1.0f, 2000);

		worldParams.camera = m_Camera.get();
		m_Engine->AddWorld(worldParams);
	}

	void Editor::Update(double deltaTime)
	{
		
	}

	void Editor::Stop()
	{
		AppBase::Stop();
	}
}