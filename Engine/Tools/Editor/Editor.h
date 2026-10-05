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
	class InputManager;
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
		// Reads EditorConfig.ini's "ImportDefaultAssets" flag (treated as true if missing) and,
		// if set, imports the engine's default/dummy content (fallback textures, the default
		// material, the test cube) from SourceAssets, then clears the flag so it isn't repeated
		// on every subsequent startup. Called once from Initialize().
		void ImportDefaultAssetsIfNeeded();

		WindowHandle m_PrimaryWindow{};
		URef<Camera> m_Camera;
		WindowModule* m_WindowModule{};
		AssetManager* m_AssetManager{};
		WorldManager* m_WorldManager{};
		RendererAPI* m_RendererAPI{};
		Handle m_LevelEditorWorld{};
		GUIModule* m_GUIModule{};
		InputManager* m_InputManager{};
		// Both constructed in Initialize() rather than the member init list - they need
		// m_RendererAPI, which isn't resolved from the module manager until then.
		URef<EditorUI> m_EditorUI;
		URef<EditorViewport> m_EditorViewport;
	};
	
}