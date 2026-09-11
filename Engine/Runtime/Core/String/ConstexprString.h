#pragma once

#include <cstddef>

namespace tyr
{
    template<size_t N>
    struct ConstexprString
    {
        char data[N + 1]{};

        // Construct from string literal
        constexpr ConstexprString(const char(&str)[N + 1])
        {
            for (size_t i = 0; i < N; ++i)
                data[i] = str[i];
            data[N] = '\0';
        }

        constexpr size_t size() const { return N; }
    };
}