#pragma once

#include "Base/Base.h"
#include "Containers/Containers.h"
#include "String/StringTypes.h"
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
	struct IsList : std::false_type {};

	template<typename T>
	struct IsList<List<T>> : std::true_type {};

	template<typename T>
	struct IsHashMap : std::false_type {};

	template<typename K, typename V, typename H>
	struct IsHashMap<HashMap<K, V, H>> : std::true_type {};

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
        static constexpr uint capacity = C;
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
    struct CArrayTraits;

    template<typename ElementType, size_t N>
    struct CArrayTraits<ElementType[N]>
    {
        using elementType = ElementType;
        static constexpr size_t capacity = N;
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
   
    // Note: __FUNCSIG__ and __PRETTY_FUNCTION__ are both string literals — which are stored in static memory so safe
    // to return a StringView
    template<typename T>
    constexpr String GetTypeName()
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
        return String(typeName);
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
    constexpr String GetFieldTypeName(FieldType Class::* fieldPtr)
    {
        using T = typename FieldTypeResolver<FieldType Class::*>::type;
        return GetTypeName<T>();
    }

    template<typename Class, typename FieldType>
    constexpr size_t GetFieldOffset(FieldType Class::* fieldPtr)
    {
        return reinterpret_cast<size_t>(&(reinterpret_cast<Class*>(nullptr)->*fieldPtr));
    }
}

