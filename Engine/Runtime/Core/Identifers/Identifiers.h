
#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include <functional>

namespace tyr
{
    class Id64
    {
    public:
        Id64(const char* input);

        uint64 GetValue() const { return m_Value; }

    private:
        uint64 m_Value;
    };

    class Id32
    {
    public:
        Id32(const char* input);

        uint GetValue() const { return m_Value; }

    private:
        uint m_Value;
    };

    typedef Id64 AssetID;
}

namespace std
{
    template <>
    struct hash<tyr::Id64>
    {
        size_t operator()(const tyr::Id64& id) const noexcept
        {
            return std::hash<tyr::uint64>{}(id.GetValue());
        }
    };

    template <>
    struct hash<tyr::Id32>
    {
        size_t operator()(const tyr::Id32& id) const noexcept
        {
            return std::hash<tyr::uint>{}(id.GetValue());
        }
    };
}
