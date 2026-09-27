#pragma once

#include "App/AppBase.h"
#include "Window/WindowHandle.h"

namespace tyr
{
	class WindowModule;
	class WorldManager;
	class Camera;
}

class App final : public tyr::AppBase
{
public:
	App();
	~App();

	void Initialize() override;
	void Update(float deltaTime) override;
	void Shutdown() override;
	bool WantsExit() const override;

private:
	tyr::WindowHandle m_PrimaryWindow{};
	tyr::URef<tyr::Camera> m_Camera;
	tyr::WindowModule* m_WindowModule{};
	tyr::WorldManager* m_WorldManager{};
	tyr::Handle m_MainWorld{};
};

