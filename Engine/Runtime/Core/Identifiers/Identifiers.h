#pragma once

#include "Base/Base.h"
#include "Hashing.h"

namespace tyr
{
    template<typename T, T offsetBasis, T prime>
    class Identifier final
    {
    public:
        constexpr Identifier(T hash = 0)
            : m_Hash(hash)
        {
        }

        constexpr explicit Identifier(const char* str)
        {
            m_Hash = FNV1aHash<T, offsetBasis, prime>(str);
        }

        // Useful for a substring from a string view
        constexpr Identifier(const char* str, uint size)
        {
            m_Hash = FNV1aHash<T, offsetBasis, prime>(str, size);
        }

        constexpr Identifier(const Guid& guid)
        {
            m_Hash = FNV1aHash<T, offsetBasis, prime>(guid);
        }

        // No user-provided copy-assignment - letting the compiler generate one (which just
        // assigns m_Hash, identical to what this hand-written one did minus a pointless
        // self-assignment guard - assigning a trivial T to itself is harmless) keeps
        // Identifier, and everything built on it (Id32/Id64/AssetID/Name...), trivially
        // copyable. That matters for ECS components (see ComponentReflection.h) that need
        // to be memcpy-safe.
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
        T m_Hash;
    };

    using Id32 = Identifier<uint, c_FNVOffsetBasis32, c_FNVPrime32>;

    using Id64 = Identifier<uint64, c_FNVOffsetBasis64, c_FNVPrime64>;
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
