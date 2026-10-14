#pragma once

#include "App/AppBase.h"
#include "Window/WindowHandle.h"
#include "World/Camera.h"
#include "Gameplay/FirstPersonController.h"
#include "Gameplay/Interaction.h"

namespace tyr
{
	class WindowModule;
	class WorldManager;
	class InputManager;
}

class App final : public tyr::AppBase
{
public:
	// When embedded, the app runs inside the editor's window instead of creating its own.
	App(bool embedded);
	~App();

	void Initialize() override;
	void Update(float deltaTime) override;
	void Shutdown() override;
	bool WantsExit() const override;

private:
	// Puts a player character at the level's player start and takes control of it.
	void SpawnPlayer();

	tyr::Camera m_Camera;
	tyr::WindowHandle m_PrimaryWindow{};
	tyr::WindowModule* m_WindowModule{};
	tyr::WorldManager* m_WorldManager{};
	tyr::InputManager* m_InputManager{};
	tyr::FirstPersonController m_Controller;
	tyr::InteractionController m_Interaction;
	tyr::Handle m_MainWorld{};
	bool m_Embedded;
};
