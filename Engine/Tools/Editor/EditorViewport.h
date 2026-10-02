#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	class RendererAPI;

	// Unreal-style 3D viewport panel: displays GeometryPass's output (a dedicated offscreen
	// texture - see RendererAPI::GetOrCreateRenderViewportTexture) inside a floating, resizable
	// ImGui window, with a toggle to maximize it over the rest of the editor. Purely a reader of
	// whichever RenderViewport it's given each Draw() call - the viewport itself is created/
	// destroyed by World, not this class (see RenderViewport's own comment), so there's nothing
	// for this class to release on shutdown any more.
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
