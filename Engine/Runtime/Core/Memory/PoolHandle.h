#pragma once

#include "Base/Base.h"

namespace tyr
{
    struct PoolHandle
    {
        static constexpr uint c_InvalidIndex = UINT_MAX;
        uint index = c_InvalidIndex;
        uint generation = 0;

        explicit operator bool() const
        {
            return index != c_InvalidIndex;
        }

        bool operator ==(const PoolHandle& handle) const
        {
            return handle.index == index && handle.generation == generation;
        }
    };

    using Handle = PoolHandle;

#define TYR_CREATE_HANDLE_TYPE(Name) \
    struct Name \
    { \
        Handle h; \
        Name() = default; \
        explicit Name(Handle handle) : h(handle) {} \
        explicit operator bool() const \
        { \
            return static_cast<bool>(h); \
        } \
        bool operator ==(const Name& other) const \
        { \
            return h == other.h; \
        } \
    };
}

