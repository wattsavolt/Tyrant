/// Copyright (c) 2023 Aidan Clear

#include "App.h"
#include "BuildConfig.h"
#include "Window/WindowModule.h"
#include "Window/WindowDesc.h"
#include "World/WorldModule.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "Platform/Platform.h"
#include "Level/LevelFile.h"
#include "AssetSystem/AssetUtil.h"
#include "Config/Config.h"

using namespace tyr;

TYR_EXPORT AppBase* TYR_STDCALL CreateApp(bool embedded)
{
	// #pragam comment line below prevents having to use extern "C"
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
	return new App(embedded);
}


App::App(bool embedded)
	: m_Embedded(embedded)
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

	// Pulled back from the origin along -Z, looking down +Z toward it.
	m_Camera = Camera(Vector3(0, 0, -3), Vector3::c_Up, Vector3::c_Forward, 90, 1.0f, 2000);

	WorldConfig worldParams{};
	worldParams.camera = &m_Camera;

	if (m_Embedded)
	{
		// Draws into the host's window instead of opening a second one.
		const World& hostWorld = m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld());
		worldParams.windowHandle = hostWorld.windowHandle;
		worldParams.osWindowHandle = hostWorld.osWindowHandle;

		m_MainWorld = m_WorldManager->AddWorld(worldParams);
		m_WorldManager->SetActiveWorld(m_MainWorld);
	}
	else
	{
		// Get project related properties from the config later
		WindowDesc desc;
		desc.showFlag = 1;
		desc.name = c_AppName;
		Platform::GetMaxWindowResolution(desc.width, desc.height);

		m_MainWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);

		char configPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullConfigPath(configPath, "GameConfig.ini");
		const Config gameConfig(configPath);
		if (gameConfig.HasValue("StartLevel"))
		{
			SetStartLevel(gameConfig.GetValue("StartLevel").CStr());
		}
	}

	if (m_StartLevel.Size() > 0)
	{
		LevelFile::Load(m_StartLevel.CStr(), *m_WorldManager, m_MainWorld);
	}
}

void App::Update(float deltaTime)
{

}

void App::Shutdown()
{
	if (m_Embedded)
	{
		m_WorldManager->RemoveWorld(m_MainWorld);
	}
	else
	{
		DestroyPrimaryWorld(*m_WindowModule, *m_WorldManager, m_MainWorld, m_PrimaryWindow);
	}
	m_MainWorld = {};
	m_WorldManager = nullptr;
}

bool App::WantsExit() const
{
	TYR_ASSERT(!m_Embedded);
	return m_WindowModule->IsWindowActive(m_PrimaryWindow);
}
