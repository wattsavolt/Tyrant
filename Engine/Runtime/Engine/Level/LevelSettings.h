#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	// Settings for a whole level, saved with it.
	struct LevelSettings
	{
		// Flat light added everywhere, until indirect lighting replaces it.
		float ambient = 0.15f;
		// Flat light added only where a surface can see the sky, such as outdoors on a cloudy day.
		float skyAmbient = 0.0f;

		// A cubemap made by importing an HDR image as a sky. Without one the sky is black.
		AssetID skyTexture;
		Vector3 skyTint = Vector3(1.0f, 1.0f, 1.0f);
		float skyIntensity = 1.0f;
		// The sky's turn about the vertical axis, in degrees.
		float skyYaw = 0.0f;

		// Exposure is in EV100. Auto exposure follows how bright the view is, within the minimum
		// and maximum. The lower the minimum, the more a dark view is brightened.
		bool autoExposure = true;
		// Each step up doubles the brightness.
		float exposureCompensation = 0.0f;
		float manualExposure = 0.0f;
		float minExposure = -1.0f;
		float maxExposure = 12.0f;
		// How quickly auto exposure adapts, as one over the seconds it takes to get most of the way.
		float exposureBrightenSpeed = 1.5f;
		float exposureDarkenSpeed = 3.0f;

		// How much bright light glows into the pixels around it, 0 for none.
		float bloomIntensity = 0.1f;
	};
}
