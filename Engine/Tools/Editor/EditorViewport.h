#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class RendererAPI;

	// Unreal-style 3D viewport panel: displays the scene's offscreen render target inside a
	// floating, resizable ImGui window, with a toggle to maximize it over the editor. Owns
	// nothing itself - purely a reader of whichever viewport it's given each Draw() call.
	class TYR_EDITOR_EXPORT EditorViewport final
	{
	public:
		EditorViewport(RendererAPI& rendererAPI);

		void Draw(RenderViewportHandle viewport);

	private:
		RendererAPI& m_RendererAPI;
		TextureHandle m_Texture;
		uint m_Width = 0;
		uint m_Height = 0;
		bool m_Maximized = false;
	};
}
