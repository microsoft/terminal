// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    _TIL_INLINEPREFIX std::wstring visualize_control_codes(std::wstring str) noexcept
    {
        for (auto& ch : str)
        {
            if (ch < 0x20)
            {
                ch += 0x2400;
            }
            else if (ch == 0x20)
            {
                ch = 0x2423; // replace space with ␣
            }
            else if (ch == 0x7f)
            {
                ch = 0x2421; // replace del with ␡
            }
        }
        return str;
    }

    _TIL_INLINEPREFIX std::wstring visualize_control_codes(std::wstring_view str)
    {
        return visualize_control_codes(std::wstring{ str });
    }

    template<typename T, typename Traits>
    constexpr bool starts_with(const std::basic_string_view<T, Traits> str, const std::basic_string_view<T, Traits> prefix) noexcept
    {
#ifdef __cpp_lib_starts_ends_with
#error This code can be replaced in C++20, which natively supports .starts_with().
#endif
        return str.size() >= prefix.size() && Traits::compare(str.data(), prefix.data(), prefix.size()) == 0;
    };

    constexpr bool starts_with(const std::string_view str, const std::string_view prefix) noexcept
    {
        return starts_with<>(str, prefix);
    };

    constexpr bool starts_with(const std::wstring_view str, const std::wstring_view prefix) noexcept
    {
        return starts_with<>(str, prefix);
    };

    template<typename T, typename Traits>
    constexpr bool ends_with(const std::basic_string_view<T, Traits> str, const std::basic_string_view<T, Traits> prefix) noexcept
    {
#ifdef __cpp_lib_ends_ends_with
#error This code can be replaced in C++20, which natively supports .ends_with().
#endif
        return str.size() >= prefix.size() && Traits::compare(str.data() + (str.size() - prefix.size()), prefix.data(), prefix.size()) == 0;
    };

    constexpr bool ends_with(const std::string_view str, const std::string_view prefix) noexcept
    {
        return ends_with<>(str, prefix);
    };

    constexpr bool ends_with(const std::wstring_view str, const std::wstring_view prefix) noexcept
    {
        return ends_with<>(str, prefix);
    };
}
