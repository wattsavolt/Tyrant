#pragma once

#include "App/AppBase.h"
#include "EditorMacros.h"
#include "Window/WindowHandle.h"

namespace tyr
{
	class WindowModule;
	class Camera;
	class AssetManager;
	class WorldManager;
	class World;
	class TYR_EDITOR_EXPORT Editor final : public AppBase
	{
	public:
		Editor();
		~Editor();

		void Initialize() override;
		void Update(float deltaTime) override;
		void Shutdown() override;
		bool WantsExit() const override;

	private:
		WindowHandle m_PrimaryWindow{};
		URef<Camera> m_Camera;
		WindowModule* m_WindowModule{};
		AssetManager* m_AssetManager{};
		WorldManager* m_WorldManager{};
		Handle m_LevelEditorWorld{};
	};
	
}