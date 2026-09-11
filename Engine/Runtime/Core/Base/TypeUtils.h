#pragma once

#include <cstdint>
#include <type_traits>
#include <concepts>

namespace tyr
{
    template <typename T>
    constexpr bool IsDefaultConstructibleObject =
        !std::is_abstract_v<T> &&
        !std::is_reference_v<T> &&
        !std::is_pointer_v<T> &&
        std::is_default_constructible_v<T>;

    template <typename T>
    concept EqualityComparable = requires(T a, T b) 
    {
        { a == b } -> std::convertible_to<bool>;
    };
}
