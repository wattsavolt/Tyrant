#pragma once

#include "Base/Base.h"
#include "Containers/Containers.h"
#include "String/StringTypes.h"
#include "Identifiers/Identifiers.h"
#include <type_traits>

namespace tyr
{
    template<typename T>
    struct IsLocalArray : std::false_type {};

    template<typename T, uint C>
    struct IsLocalArray<LocalArray<T, C>> : std::true_type {};

	template<typename T>
	struct IsArray : std::false_type {};

	template<typename T>
	struct IsArray<Array<T>> : std::true_type {};

	template<typename T>
	struct IsHashMap : std::false_type {};

	template<typename K, typename V, typename H>
	struct IsHashMap<HashMap<K, V, H>> : std::true_type {};

    template<typename T>
    struct IsLocalString : std::false_type {};

    template<uint N>
    struct IsLocalString<LocalString<N>> : std::true_type {};

    template<typename T>
    struct IsConstCharPtr : std::is_same<const char*, std::decay_t<T>> {};

    template <typename T, template <typename...> class Template>
    struct IsSpecialization : std::false_type {};

    template <template <typename...> class Template, typename... Args>
    struct IsSpecialization<Template<Args...>, Template> : std::true_type {};

    template<typename T>
    struct LocalArrayTraits;

    template<typename T, uint C>
    struct LocalArrayTraits<LocalArray<T, C>>
    {
        using elementType = T;
        static constexpr uint c_Capacity = C;
    };

    template<typename T>
    struct ArrayTraits;

    template<typename T>
    struct ArrayTraits<Array<T>>
    {
        using elementType = T;
    };

    template<typename T>
    struct HashMapTraits;

    template <typename K, typename V, typename H, typename C>
    struct HashMapTraits<HashMap<K, V, H, C>>
    {
        using keyType = K;
        using valueType = V;
        using hashType = H;
        using compareType = C;
    };

    template<typename T>
    struct LocalStringTraits;

    template<uint N>
    struct LocalStringTraits<LocalString<N>>
    {
        // Capacity - 1 as excludes null character
        static constexpr uint c_MaxSize = N;
    };

    template<typename T>
    struct CArrayTraits;

    template<typename ElementType, size_t N>
    struct CArrayTraits<ElementType[N]>
    {
        using elementType = ElementType;
        static constexpr size_t c_Capacity = N;
    };

    template<typename T>
    struct IsIdentifier : std::false_type {};

    template<typename T, T offsetBasis, T prime>
    struct IsIdentifier<Identifier<T, offsetBasis, prime>> : std::true_type {};

    template<typename T>
    struct IdentifierTraits;

    template<typename T, T offsetBasis, T prime>
    struct IdentifierTraits<Identifier<T, offsetBasis, prime>>
    {
        using hashType = T;
        static constexpr size_t c_OffsetBasis = offsetBasis;
        static constexpr size_t c_Prime = prime;
    };

    constexpr StringView StripNamespace(StringView typeName) 
    {
        const size_t pos = typeName.rfind("::");
        return (pos != StringView::npos) ? typeName.substr(pos + 2) : typeName;
    }

    constexpr StringView RemoveStructOrClass(StringView typeName)
    {
        if (typeName.starts_with("struct "))
        {
            return typeName.substr(7);  // Remove "struct "
        }

        if (typeName.starts_with("class "))
        {
            return typeName.substr(6);  // Remove "class "
        }

        return typeName;
    }
   
    template<typename T>
    constexpr StringView GetTypeName()
    {
        StringView typeName;
#if defined(_MSC_VER)  // MSVC uses __FUNCSIG__
        constexpr StringView sig = __FUNCSIG__;
        constexpr StringView key = "GetTypeName<";
        size_t start = sig.find(key);
        if (start == StringView::npos)
        {
            return "Unknown";
        }

        start += key.length();  // Move past "GetTypeName<"

        // Find end of type name (typically at the closing '>')
        const size_t end = sig.rfind('>');  // rfind ensures it works with nested templates

        typeName = RemoveStructOrClass(StripNamespace(sig.substr(start, end - start)));

#elif defined(__GNUC__) || defined(__clang__)  // GCC & Clang use __PRETTY_FUNCTION__
        constexpr StringView sig = __PRETTY_FUNCTION__;
        constexpr StringView key = "Type = ";

        size_t start = sig.find(key);
        if (start == StringView::npos)
        {
            return "Unknown";
        }

        start += key.length();
        const size_t end = sig.find_first_of("];", start);  // Stop at ] or ;

        typeName = RemoveStructOrClass(StripNamespace(sig.substr(start, end - start)));

#else
#error "Unsupported compiler"
#endif
        return typeName;
    }

    template<typename T>
    constexpr Id64 GetTypeID()
    {
        const StringView typeName = GetTypeName<T>();
        return Id64(typeName.data(), static_cast<uint>(typeName.size()));
    }

    template <typename T>
    constexpr StringView GetTypeName(const T&)
    {
        return GetTypeName<T>();
    }

    template <typename T>
    constexpr Id64 GetTypeID(const T&)
    {
        return GetTypeID<T>();
    }

    template<typename T>
    struct FieldTypeResolver;

    // General case: field is not an array
    template<typename Class, typename Type>
    struct FieldTypeResolver<Type Class::*>
    {
        using type = Type;
    };

    // C-style array case: extract element type from arrays
    template<typename Class, typename ElementType, size_t N>
    struct FieldTypeResolver<ElementType(Class::*)[N]>
    {
        using type = ElementType;
    };

    template <typename Class, typename FieldType>
    constexpr StringView GetFieldTypeName(FieldType Class::* fieldPtr)
    {
        using T = typename FieldTypeResolver<FieldType Class::*>::type;
        return GetTypeName<T>();
    }

    template <typename Class, typename FieldType>
    constexpr Id64 GetFieldTypeID(FieldType Class::* fieldPtr)
    {
        using T = typename FieldTypeResolver<FieldType Class::*>::type;
        return GetTypeID<T>();
    }

    template<typename Class, typename FieldType>
    constexpr size_t GetFieldOffset(FieldType Class::* fieldPtr)
    {
        return reinterpret_cast<size_t>(&(reinterpret_cast<Class*>(nullptr)->*fieldPtr));
    }
}

