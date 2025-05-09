
#pragma once

#include "Identifiers.h"
#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Hashing.h"

namespace tyr
{
    Id64::Id64(const char* input)
    {
        m_Value = FNV1aHash<uint64, FNV_OFFSET_BASIS_64, FNV_PRIME_64>(input);
    }

    Id32::Id32(const char* input)
    {
        m_Value = FNV1aHash<uint, FNV_OFFSET_BASIS_32, FNV_PRIME_32>(input);
    }
}
