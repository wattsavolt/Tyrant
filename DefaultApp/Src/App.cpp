/// Copyright (c) 2023 Aidan Clear 

#include "App.h"
#include "BuildConfig.h"
#include "Window/WindowModule.h"
#include "Window/WindowDesc.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Camera.h"
#include "Platform/Platform.h"

using namespace tyr;

TYR_EXPORT AppBase* TYR_STDCALL CreateApp()
{
	// #pragam comment line below prevents having to use extern "C"
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
	return new App();
}


App::App()
{

}

App::~App()
{

}

void App::Initialize()
{
	TYR_GET_MODULE(WindowModule, m_WindowModule);

	WorldModule* worldModule;
	TYR_GET_MODULE(WorldModule, worldModule);
	m_WorldManager = worldModule->GetWorldManager();

	// Get project related properties from the config later
	WindowDesc desc;
	desc.showFlag = 1;
	desc.name = c_AppName;
	Platform::GetMaxWindowResolution(desc.width, desc.height);

	// Pulled back from the origin along -Z, looking down +Z (c_Forward) toward it - matches
	// Editor::Initialize's camera setup.
	m_Camera = MakeURef<Camera>(Vector3(0, 0, -3), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

	WorldConfig worldParams{};
	worldParams.camera = m_Camera.get();

	m_MainWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);
}

void App::Update(float deltaTime)
{

}

void App::Shutdown()
{
	m_WorldManager->RemoveWorld(m_MainWorld);
	m_MainWorld = {};
	m_WorldManager = nullptr;
	m_WindowModule->DestroyWindow(m_PrimaryWindow);
}

bool App::WantsExit() const
{
	return m_WindowModule->IsWindowActive(m_PrimaryWindow);
}