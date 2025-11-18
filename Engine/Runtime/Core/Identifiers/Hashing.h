#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Guid.h"

namespace tyr
{
    static constexpr uint64 c_FNVOffsetBasis64 = 14695981039346656037;
    static constexpr uint64 c_FNVSignedOffsetBasis64 = -3750763034362895579;
    static constexpr uint64 c_FNVPrime64 = 1099511628211;

    static constexpr uint64 c_FNVOffsetBasis32 = 2166136261;
    static constexpr uint64 c_FNVSignedOffsetBasis32 = -2128831035;
    static constexpr uint64 c_FNVPrime32 = 16777619;

    template<typename T, T offsetBasis, T prime>
    constexpr T FNV1aHash(const char* const input)
    {
        T hash = offsetBasis;
        for (uint i = 0; input[i] != '\0'; ++i)
        {
            hash = (hash ^ T(input[i])) * prime;
        }
        return hash;
    }

    // Handy for a substring
    template<typename T, T offsetBasis, T prime>
    constexpr T FNV1aHash(const char* const input, uint size)
    {
        T hash = offsetBasis;
        for (uint i = 0; i < size; ++i)
        {
            hash = (hash ^ T(input[i])) * prime;
        }
        return hash;
    }

    template<typename T, T offsetBasis, T prime>
    constexpr T FNV1aHash(const Guid& guid)
    {
        T hash = offsetBasis;

        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&guid);
        for (size_t i = 0; i < sizeof(Guid); ++i)
        {
            hash = (hash ^ bytes[i]) * prime;
        }

        return hash;
    }
}
