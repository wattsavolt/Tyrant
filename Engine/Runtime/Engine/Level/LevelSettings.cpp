#include "LevelSettings.h"
#include "Reflection/Reflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(LevelSettings, 0);
		TYR_REFL_FIELD(&LevelSettings::ambient, "Ambient", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::skyTexture, "Sky Texture", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::skyTint, "Sky Tint", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::skyIntensity, "Sky Intensity", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::skyYaw, "Sky Yaw", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::autoExposure, "Auto Exposure", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::exposureCompensation, "Exposure Compensation", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::manualExposure, "Manual Exposure", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::minExposure, "Min Exposure", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::maxExposure, "Max Exposure", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::exposureBrightenSpeed, "Exposure Brighten Speed", true, true, true);
		TYR_REFL_FIELD(&LevelSettings::exposureDarkenSpeed, "Exposure Darken Speed", true, true, true);
	TYR_REFL_CLASS_END();
}
