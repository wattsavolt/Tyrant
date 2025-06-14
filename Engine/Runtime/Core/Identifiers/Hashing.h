
#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Guid.h"

namespace tyr
{
#define TYR_FNV_OFFSET_BASIS_64 14695981039346656037
#define TYR_FNV_SIGNED_OFFSET_BASIS_64 -3750763034362895579
#define TYR_FNV_PRIME_64 1099511628211
#define TYR_FNV_OFFSET_BASIS_32 2166136261
#define TYR_FNV_SIGNED_OFFSET_BASIS_32 -2128831035
#define TYR_FNV_PRIME_32 16777619

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

    template<typename T, T offsetBasis, T prime>
    constexpr T FNV1aHash(const Guid& guid)
    {
        T hash = offsetBasis;

        hash = (hash ^ T(guid.data1)) * prime;

        hash = (hash ^ T(guid.data2)) * prime;

        hash = (hash ^ T(guid.data3)) * prime;

        for (int i = 0; i < 8; ++i)
        {
            hash = (hash ^ T(guid.data4[i])) * prime;
        }

        return hash;
    }
}
