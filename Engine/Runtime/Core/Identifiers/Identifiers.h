#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Reflection/PrivateReflection.h"

namespace tyr
{
    struct Guid;
    class TYR_CORE_EXPORT Id64
    {
    public:
        Id64();
        Id64(const char* input);
        Id64(const String& input);
        Id64(const Guid& guid);
        Id64(uint64 hash);

        uint64 GetHash() const { return m_Hash; }

        bool operator== (const Id64& other) const
        {
            return m_Hash == other.m_Hash;
        }

        bool operator< (const Id64& other) const
        {
            return m_Hash < other.m_Hash;
        }

    private:
        TYR_REFL_PRIVATE_ACCESS(Id64);
        uint64 m_Hash;
    };

    class TYR_CORE_EXPORT Id32
    {
    public:
        Id32();
        Id32(const char* str);
        Id32(const String& str);
        Id32(const Guid& guid);
        Id32(uint hash);

        uint GetHash() const { return m_Hash; }

        bool operator== (const Id32& other) const
        {
            return m_Hash == other.m_Hash;
        }

        bool operator< (const Id32& other) const
        {
            return m_Hash < other.m_Hash;
        }

    private:
        TYR_REFL_PRIVATE_ACCESS(Id32);
        uint m_Hash;
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
            return std::hash<tyr::uint64>{}(id.GetHash());
        }
    };

    template <>
    struct hash<tyr::Id32>
    {
        size_t operator()(const tyr::Id32& id) const noexcept
        {
            return std::hash<tyr::uint>{}(id.GetHash());
        }
    };
}
