#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"
#include "EditorCameraController.h"

namespace tyr
{
	class RendererAPI;
	class InputManager;
	class Camera;

	// Unreal-style 3D viewport panel: displays the scene's offscreen render target inside a
	// floating, resizable ImGui window, with a toggle to maximize it over the editor. Also owns
	// the fly camera controller for this panel - everything else is purely a reader of whichever
	// viewport it's given each Draw() call.
	class TYR_EDITOR_EXPORT EditorViewport final
	{
	public:
		EditorViewport(RendererAPI& rendererAPI);

		void Draw(RenderViewportHandle viewport, InputManager& inputManager, Camera& camera, float deltaTime);

	private:
		RendererAPI& m_RendererAPI;
		TextureHandle m_Texture;
		uint m_Width = 0;
		uint m_Height = 0;
		bool m_Maximized = false;
		EditorCameraController m_CameraController;
	};
}
