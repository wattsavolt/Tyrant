#pragma once

#include "RenderBase/RenderHandles.h"
#include "Rendering/RenderConstants.h"

namespace tyr
{
	// One buffered slot's own G-buffer/colour/depth texture set for a single scene's viewport -
	// one full set per buffered RenderFrame slot, each (re)creating only its own entry, on its
	// own turn, when the requested size changes.
	struct RenderViewportTextureData
	{
		TextureHandle colourTexture;
		// gbufferAlbedoAO: albedo.rgb, ambient occlusion.a
		// gbufferNormalRoughMetal: octahedral-encoded world-space normal.rg, roughness.b, metallic.a
		// gbufferMotion: screen-space motion vector (current NDC - reprojected previous NDC).
		TextureHandle gbufferAlbedoAO;
		TextureHandle gbufferNormalRoughMetal;
		TextureHandle gbufferMotion;
		TextureHandle depthBuffer;
		uint width = 0;
		uint height = 0;
		// Set whenever this slot's own targets are (re)created, consumed once by the geometry
		// pass to run the one-time "undefined -> real layout" transition a fresh image needs
		// before its first use as an attachment - reset back to false right after.
		bool isNew = false;
	};

	// A scene's own render target set. Like RenderWindow, this lives in its own pool on Renderer
	// (Renderer::m_RenderViewportPool), not RenderRegistry - a Scene holds a RenderViewportHandle
	// into this pool (see Scene::renderViewport) the same way it already holds a
	// RenderWindowHandle, and no two scenes ever share one.
	//
	// This decouples a scene's G-buffer/colour targets from any particular editor ImGui panel
	// existing at all: World creates/destroys a RenderViewport for the lifetime of its scene (see
	// WorldManager::InitWorld/ShutdownWorld), regardless of whether anything is currently
	// displaying it, and the editor's viewport panel (EditorViewport) is just one possible reader
	// of whichever scene happens to be active. Earlier, a single global ViewportTargets[] set was
	// read fresh every single tick via a per-frame "snapshot" copy out of RenderResources
	// regardless of whether anything had actually changed - a real bug, since an editor ImGui
	// layout hiccup on any one tick (producing a transient zero-sized content region for that
	// tick's buffered slot) silently produced a black frame with no validation/error signal, and
	// switching which scene was active was never accounted for at all. Resizes are now explicit,
	// infrequent events instead of an unconditional per-tick re-derivation, and a scene that isn't
	// currently active just keeps whatever targets it last had, undisturbed.
	//
	// Resize propagation: RendererAPI::GetOrCreateRenderViewportTexture resizes the CURRENT
	// renderFrameIndex's own textureData slot immediately (safe - nothing else touches a slot until
	// its own turn comes back around), then records the new size here in requestedWidth/
	// requestedHeight and marks the other two slots' pendingResize[slot] true. Renderer::Render
	// checks its own tick's slot's pendingResize flag and, if set, resizes that slot to whatever
	// requestedWidth/requestedHeight currently holds (always the latest request - a later resize
	// simply overwrites requestedWidth/requestedHeight and leaves the flag set, so an
	// already-pending slot naturally picks up the newest size once its turn comes, with no list/
	// queue needed) before clearing the flag.
	struct RenderViewport
	{
		RenderViewportTextureData textureData[RenderConstants::c_BufferedFrameCount];

		// The latest size anything has actually requested for this viewport (main thread only) -
		// see this struct's own comment on resize propagation above.
		uint requestedWidth = 0;
		uint requestedHeight = 0;
		// Indexed by renderFrameIndex, same as textureData - see this struct's own comment above.
		bool pendingResize[RenderConstants::c_BufferedFrameCount] = {};
	};
}
