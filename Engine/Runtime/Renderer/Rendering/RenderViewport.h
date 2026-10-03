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
		// Texture2DArray, RenderConstants::c_MaxShadowSlots layers - raw per-light ray-traced
		// visibility (shadowMasksRaw) and the denoised/temporally-accumulated result
		// (shadowMasks) that both this tick's lighting pass and next tick's denoiser read as
		// history.
		TextureHandle shadowMasksRaw;
		TextureHandle shadowMasks;
		// TAA's resolved output - what the editor/GUI actually displays when TAA is enabled
		// (colourTexture otherwise). Reads the previous buffered slot's own resolvedColourTexture
		// as history, the same cross-slot-reuse pattern the shadow denoiser already uses.
		TextureHandle resolvedColourTexture;
		uint width = 0;
		uint height = 0;
		// Set whenever this slot's own targets are (re)created, consumed once by the geometry
		// pass to run the one-time "undefined -> real layout" transition a fresh image needs
		// before its first use as an attachment - reset back to false right after.
		bool isNew = false;
	};

	// A scene's own render target set. Like RenderWindow, this lives in its own pool on
	// Renderer, not RenderRegistry - a Scene holds a handle into this pool, and no two scenes
	// ever share one.
	//
	// This decouples a scene's G-buffer/colour targets from any particular editor ImGui panel
	// existing at all - a scene's RenderViewport persists for the scene's whole lifetime
	// regardless of whether anything is currently displaying it.
	//
	// Resize propagation: the current slot resizes immediately; the other two slots get the
	// new size recorded in requestedWidth/requestedHeight with their pendingResize flag set,
	// and each picks it up on its own next turn, always using the latest requested size.
	struct RenderViewport
	{
		RenderViewportTextureData textureData[RenderConstants::c_BufferedFrameCount];

		// The latest size anything has actually requested for this viewport (main thread only).
		uint requestedWidth = 0;
		uint requestedHeight = 0;
		// Indexed by renderFrameIndex, same as textureData.
		bool pendingResize[RenderConstants::c_BufferedFrameCount] = {};
	};
}
