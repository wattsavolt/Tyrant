#include "Level.h"


namespace tyr
{
    TYR_REFL_CLASS_START(Location, 0);
    TYR_REFL_FIELD(&Location::textures, "Textures", true, true, true);
    TYR_REFL_FIELD(&Location::materials, "Materials", true, true, true);
    TYR_REFL_CLASS_END();
}