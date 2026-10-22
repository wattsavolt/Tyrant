#pragma once

#include "RenderBase/RenderHandles.h"
#include "Rendering/RenderConstants.h"
#include "Math/Vector4.h"

namespace tyr
{
	// An infinite grid on a flat, level plane, drawn over a viewport's final image. Hidden
	// behind anything the scene draws closer to the camera.
	struct ViewportGridDesc
	{
		bool enabled = false;
		// The height of the plane the grid lies on.
		float planeHeight = 0.0f;
		float cellSize = 1.0f;
		// How many cells apart the brighter major lines are.
		uint majorLineEvery = 10;
		// Line widths are fractions of a cell (minor, axis) or of a major cell (major).
		float minorLineWidth = 0.01f;
		float majorLineWidth = 0.0025f;
		float axisLineWidth = 0.025f;
		// Linear colours, with how opaque the lines are in alpha.
		Vector4 minorLineColour = Vector4(0.25f, 0.25f, 0.25f, 0.45f);
		Vector4 majorLineColour = Vector4(0.4f, 0.4f, 0.4f, 0.7f);
		// The X axis runs along z = 0 and the Z axis along x = 0.
		Vector4 xAxisColour = Vector4(0.8f, 0.1f, 0.1f, 0.9f);
		Vector4 zAxisColour = Vector4(0.1f, 0.25f, 0.9f, 0.9f);
		// The grid fades out between these distances from the camera, in major cells.
		float fadeStart = 10.0f;
		float fadeEnd = 20.0f;
	};

	enum class ViewportCaptureStatus : uint8
	{
		// The GPU hasn't finished the frame it was taken in yet.
		Waiting,
		Ready,
		// Nothing was drawn to capture, or the ID is unknown.
		Failed
	};

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
		// Light surfaces give off themselves, rgb.
		TextureHandle gbufferEmissive;
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
		// Translucent surfaces' lit colours added up by weight, with how much of what's behind
		// still shows in alpha, and their coverage added up by weight.
		TextureHandle translucentAccumulation;
		TextureHandle translucentWeight;
		// The final image with the grid drawn over it, kept apart from the TAA history so the grid
		// never smears. Only exists while the viewport's grid is enabled.
		TextureHandle overlayColourTexture;
		// The exposed, tone mapped image the viewport shows, in linear colour where 1 is paper white.
		TextureHandle displayColourTexture;
		// Bloom's chain, each half the size of the one before, starting at half the viewport's size.
		TextureHandle bloom[RenderConstants::c_BloomLevels];
		uint width = 0;
		uint height = 0;
		// Set whenever this slot's own targets are (re)created, consumed once by the geometry
		// pass to run the one-time "undefined -> real layout" transition a fresh image needs
		// before its first use as an attachment - reset back to false right after.
		bool isNew = false;
		// Same as isNew, for an overlay texture created on its own when the grid is turned on.
		bool overlayIsNew = false;
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
		// Main thread only - snapshotted into the RenderFrame for the worker.
		ViewportGridDesc grid;
		// Sized to its window and shown on all of it, rather than in an editor panel. Main thread only.
		bool presentToWindow = false;
		// Whether its exposure has been worked out before, so it can adapt from there. Render worker only.
		bool exposureHasHistory = false;
	};
}
