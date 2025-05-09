
#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"

namespace tyr
{
#define FNV_OFFSET_BASIS_64 14695981039346656037
#define FNV_SIGNED_OFFSET_BASIS_64 -3750763034362895579
#define FNV_PRIME_64 1099511628211
#define FNV_OFFSET_BASIS_32 2166136261
#define FNV_SIGNED_OFFSET_BASIS_32 -2128831035
#define FNV_PRIME_32 16777619

    template<typename T, T offsetBasis, T prime>
    constexpr T FNV1aHash(const char* const input)
    {
        T hash = offsetBasis;
        for (int i = 0; input[i] != '\0'; ++i)
        {
            hash = (hash ^ T(input[i])) * prime;
        }
        return hash;
    }

}
