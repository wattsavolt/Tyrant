#pragma once

#include "App/AppBase.h"
#include "Window/WindowHandle.h"
#include "World/Camera.h"

namespace tyr
{
	class WindowModule;
	class WorldManager;
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
	tyr::Camera m_Camera;
	tyr::WindowHandle m_PrimaryWindow{};
	tyr::WindowModule* m_WindowModule{};
	tyr::WorldManager* m_WorldManager{};
	tyr::Handle m_MainWorld{};
	bool m_Embedded;
};
