#pragma once
#include "Identifiers.h"
#include "Reflection/Reflection.h"


namespace tyr
{
    TYR_REFL_CLASS_START(Id64, 0);
        TYR_REFL_FIELD(&Id64::m_Hash, "Hash", true, true, true);
    TYR_REFL_CLASS_END();

    TYR_REFL_CLASS_START(Id32, 0);
        TYR_REFL_FIELD(&Id32::m_Hash, "Hash", true, true, true);
    TYR_REFL_CLASS_END();
}
