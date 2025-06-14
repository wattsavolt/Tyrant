#pragma once

#include "Identifiers.h"
#include "Hashing.h"
#include "Reflection/Reflection.h"
#include "Guid.h"

namespace tyr
{
    TYR_REFL_CLASS_START(Id64, 0);
        TYR_REFL_FIELD(&Id64::m_Hash, "Hash", true, true);
    TYR_REFL_CLASS_END();

    TYR_REFL_CLASS_START(Id32, 0);
        TYR_REFL_FIELD(&Id32::m_Hash, "Hash", true, true);
    TYR_REFL_CLASS_END();

    Id64::Id64() : m_Hash(0) {}

    Id64::Id64(const char* str)
    {
        m_Hash = FNV1aHash<uint64, TYR_FNV_OFFSET_BASIS_64, TYR_FNV_PRIME_64>(str);
    }

    Id64::Id64(const String& str)
        : Id64(str.c_str())
    { 
    }

    Id64::Id64(const Guid& guid)
    {
        m_Hash = FNV1aHash<uint64, TYR_FNV_OFFSET_BASIS_64, TYR_FNV_PRIME_64>(guid);
    }

    Id64::Id64(uint64 hash)
        : m_Hash(hash)
    {
    }

    Id32::Id32() : m_Hash(0) {}

    Id32::Id32(const char* str)
    {
        m_Hash = FNV1aHash<uint, TYR_FNV_OFFSET_BASIS_32, TYR_FNV_PRIME_32>(str);
    }

    Id32::Id32(const String& str)
        : Id32(str.c_str())
    {

    }

    Id32::Id32(const Guid& guid)
    {
        m_Hash = FNV1aHash<uint, TYR_FNV_OFFSET_BASIS_32, TYR_FNV_PRIME_32>(guid);
    }

    Id32::Id32(uint hash)
        : m_Hash(hash)
    {
    }
}
