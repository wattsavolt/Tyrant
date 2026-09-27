#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class RendererAPI;

	// Unreal-style 3D viewport panel: displays GeometryPass's output (a dedicated offscreen
	// texture - see RendererAPI::GetOrCreateViewportTexture) inside a floating, resizable
	// ImGui window, with a toggle to maximize it over the rest of the editor.
	class TYR_EDITOR_EXPORT EditorViewport final
	{
	public:
		EditorViewport(RendererAPI& rendererAPI);

		void Draw();

		// Releases the offscreen render target - must be called before the renderer shuts down,
		// since RenderRegistry asserts every texture it created has been explicitly deleted first.
		void Shutdown();

	private:
		RendererAPI& m_RendererAPI;
		TextureHandle m_Texture;
		uint m_Width = 0;
		uint m_Height = 0;
		bool m_Maximized = false;
	};
}
