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
#include "Input/InputModule.h"
#include "Input/InputManager.h"
#include "Actor/ActorRegistry.h"
#include "ECS/Components.h"
#include "Math/Math.h"

using namespace tyr;

namespace
{
	const Id64 c_PlayerStartType("PlayerStartActor");
	const Id64 c_PlayerCharacterType("PlayerCharacterActor");
}

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

	InputModule* inputModule;
	TYR_GET_MODULE(InputModule, inputModule);
	m_InputManager = inputModule->GetInputManager();

	// Pulled back from the origin along -Z, looking down +Z toward it.
	m_Camera = Camera(Vector3(0, 0, -3), Vector3::c_Up, Vector3::c_Forward, 90, 0.05f, 2000);

	WorldConfig worldParams{};
	worldParams.camera = &m_Camera;

	if (m_Embedded)
	{
		// Draws into the host's window instead of opening a second one.
		const World& hostWorld = m_WorldManager->GetWorld(m_WorldManager->GetActiveWorld());
		worldParams.windowHandle = hostWorld.renderWindowHandle;
		worldParams.osWindowHandle = hostWorld.windowHandle;

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

		// There's no editor panel to show it in, so it fills the window.
		worldParams.presentToWindow = true;
		m_MainWorld = CreatePrimaryWorld(*m_WindowModule, *m_WorldManager, desc, worldParams, m_PrimaryWindow);

		// The game has the mouse for as long as it runs.
		m_InputManager->SetWindow(m_WindowModule, m_PrimaryWindow);
		m_InputManager->SetGameInputEnabled(true);
		m_InputManager->SetMouseCaptured(true);

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
	SpawnPlayer();
}

void App::SpawnPlayer()
{
	World& world = m_WorldManager->GetWorld(m_MainWorld);

	Vector3 position = m_FallbackStartPosition;
	float yaw = m_FallbackStartYaw;
	bool found = false;
	for (const ActorInstance& actor : world.actorInstances)
	{
		if (actor.typeID == c_PlayerStartType && world.entities.HasComponent<ComponentTransform>(actor.RootEntity()))
		{
			const Transform& start = world.entities.GetComponent<ComponentTransform>(actor.RootEntity()).world;
			const Vector3 facing = start.rotation.Rotate(Vector3::c_Forward);
			position = start.position;
			yaw = Math::Atan2(facing.x, facing.z);
			found = true;
			break;
		}
	}
	if (!found && !m_HasFallbackStart)
	{
		TYR_LOG_WARNING("The level has no PlayerStartActor, so the player starts at the origin.");
	}

	const LocalArray<Entity, c_MaxActorTypeEntities> entities = ActorRegistry::Instance().InstantiateActor(c_PlayerCharacterType, world.entities);
	if (entities.IsEmpty())
	{
		TYR_LOG_ERROR("Couldn't make the player character.");
		return;
	}
	m_WorldManager->AddActorInstance(m_MainWorld, WorldManager::CreateActorInstanceID(), c_PlayerCharacterType, "Player", "", entities);

	Transform transform;
	transform.position = position;
	transform.rotation = Quaternion::c_Identity;
	transform.scale = Vector3::c_One;
	m_WorldManager->SetActorTransform(m_MainWorld, entities[0], transform);
	m_Controller.Possess(world, entities[0], yaw);
}

void App::Update(float deltaTime)
{
	const bool hasInput = m_InputManager->IsGameInputEnabled();
	m_Controller.Update(m_WorldManager->GetWorld(m_MainWorld), *m_InputManager, deltaTime, hasInput);
	m_Interaction.Update(*m_WorldManager, m_MainWorld, m_Controller.GetCharacter(), *m_InputManager, deltaTime, hasInput);
}

void App::Shutdown()
{
	m_Controller.Release(m_WorldManager->GetWorld(m_MainWorld));
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
