#pragma once

#include "App/AppBase.h"
#include "EditorMacros.h"
#include "Window/WindowHandle.h"
#include "RenderBase/RenderHandles.h"
#include "EditorUI.h"
#include "EditorViewport.h"

namespace tyr
{
	class WindowModule;
	class Camera;
	class AssetManager;
	class WorldManager;
	class World;
	class RendererAPI;
	class GUIModule;
	class TYR_EDITOR_EXPORT Editor final : public AppBase
	{
	public:
		Editor(GUIModule& guiModule);
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
		RendererAPI* m_RendererAPI{};
		Handle m_LevelEditorWorld{};
		GUIModule* m_GUIModule{};
		EditorUI m_EditorUI;
		// Constructed in Initialize() rather than the member init list - it needs m_RendererAPI,
		// which isn't resolved from the module manager until then.
		URef<EditorViewport> m_EditorViewport;
	};
	
}