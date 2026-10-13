#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "RenderBase/RenderHandles.h"

namespace tyr
{
	// How the final image is written to the swap chain.
	enum class DisplayOutput : uint8
	{
		// sRGB, which the swap chain's format encodes.
		SDR,
		// Rec. 2020 with the PQ curve.
		HDR10,
		Count
	};

	// How images are shown on the player's display. Belongs with the game's options, not a level.
	struct DisplaySettings
	{
		// Uses HDR whenever the display is in HDR mode.
		bool hdrEnabled = true;
		// How bright white in the scene is in HDR, in nits.
		float paperWhiteNits = 200.0f;
		// How bright the UI's white is in HDR, in nits.
		float uiNits = 200.0f;
		// The brightest the image gets in HDR, in nits. Zero uses what the display reports.
		float peakNits = 0.0f;
	};

	// How a scene's image is exposed, in EV100 (Lagarde and de Rousiers 2014). Set per level.
	struct ExposureSettings
	{
		bool autoExposure = true;
		// Each step up doubles the brightness.
		float compensation = 0.0f;
		// The exposure when auto exposure is off.
		float manualEV100 = 0.0f;
		// The range auto exposure keeps to. The lower the minimum, the more dark scenes brighten.
		float minEV100 = -1.0f;
		float maxEV100 = 12.0f;
		// How quickly auto exposure adapts, as one over the seconds it takes to get most of the way.
		float brightenSpeed = 1.5f;
		float darkenSpeed = 3.0f;
	};

	// What's drawn where nothing else is. Set per level.
	struct SkySettings
	{
		// A loaded cubemap, or invalid for a black sky.
		TextureHandle texture;
		Vector3 tint = Vector3(1.0f, 1.0f, 1.0f);
		float intensity = 1.0f;
		// The turn about the vertical axis, in degrees.
		float yaw = 0.0f;
	};
}
