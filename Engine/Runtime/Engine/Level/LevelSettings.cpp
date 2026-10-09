#include "LevelSettings.h"
#include "Reflection/Reflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(LevelSettings, 0);
		TYR_REFL_FIELD(&LevelSettings::ambient, "Ambient", true, true, true);
	TYR_REFL_CLASS_END();
}
