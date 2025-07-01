#pragma once

#include "Base/Base.h"
#include "String/StringTypes.h"
#include "Hashing.h"
#include "Reflection/PrivateReflection.h"

namespace tyr
{
    template<typename T, T offsetBasis, T prime>
    class Identifier
    {
    public:
        Identifier() : m_Hash(0) {}

        Identifier(const char* str)
        {
            m_Hash = FNV1aHash<T, offsetBasis, prime>(str);
        }

        Identifier(const String& str)
            : Identifier(str.c_str())
        {
        }

        Identifier(const Guid& guid)
        {
            m_Hash = FNV1aHash<T, offsetBasis, prime>(guid);
        }

        Identifier(T hash)
            : m_Hash(hash)
        {
        }

        Identifier& operator=(const Identifier<T, offsetBasis, prime>& other)
        {
            if (this != &other)
            {
                m_Hash = other.m_Hash;
            }
            return *this;
        }

        Identifier& operator=(T hash)
        {
            m_Hash = hash;
            return *this;
        }

        T GetHash() const { return m_Hash; }

        bool operator== (const Identifier<T, offsetBasis, prime>& other) const
        {
            return m_Hash == other.m_Hash;
        }

        bool operator< (const Identifier<T, offsetBasis, prime> other) const
        {
            return m_Hash < other.m_Hash;
        }

        // Compare with T directly
        bool operator== (T hash) const
        {
            return m_Hash == hash;
        }

        bool operator!= (T hash) const
        {
            return m_Hash != hash;
        }

    private:
        TYR_REFL_PRIVATE_ACCESS(Id32);
        TYR_REFL_PRIVATE_ACCESS(Id64);
        T m_Hash;
    };

    using Id32 = Identifier<uint, c_FNVOffsetBasis32, c_FNVPrime32>;

    using Id64 = Identifier<uint64, c_FNVOffsetBasis64, c_FNVPrime64>;

    using AssetID = Id64;
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
