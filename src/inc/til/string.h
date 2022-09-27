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

    namespace details
    {
        /*
         *   ,- Invalid in Path string
         * 0b00
         *    `- Invalid in Filename
         */
        static constexpr std::array<uint8_t, 128> pathFilter{ {
            // clang-format off
            0b00 /* NUL */, 0b00 /* SOH */, 0b00 /* STX */, 0b00 /* ETX */, 0b00 /* EOT */, 0b00 /* ENQ */, 0b00 /* ACK */, 0b00 /* BEL */, 0b00 /* BS  */, 0b00 /* HT  */, 0b00 /* LF  */, 0b00 /* VT  */, 0b00 /* FF  */, 0b00 /* CR  */, 0b00 /* SO  */, 0b00 /* SI  */,
            0b00 /* DLE */, 0b00 /* DC1 */, 0b00 /* DC2 */, 0b00 /* DC3 */, 0b00 /* DC4 */, 0b00 /* NAK */, 0b00 /* SYN */, 0b00 /* ETB */, 0b00 /* CAN */, 0b00 /* EM  */, 0b00 /* SUB */, 0b00 /* ESC */, 0b00 /* FS  */, 0b00 /* GS  */, 0b00 /* RS  */, 0b00 /* US  */,
            0b00 /* SP  */, 0b00 /* !   */, 0b11 /* "   */, 0b00 /* #   */, 0b00 /* $   */, 0b00 /* %   */, 0b00 /* &   */, 0b00 /* '   */, 0b00 /* (   */, 0b00 /* )   */, 0b11 /* *   */, 0b00 /* +   */, 0b00 /* ,   */, 0b00 /* -   */, 0b00 /* .   */, 0b01 /* /   */,
            0b00 /* 0   */, 0b00 /* 1   */, 0b00 /* 2   */, 0b00 /* 3   */, 0b00 /* 4   */, 0b00 /* 5   */, 0b00 /* 6   */, 0b00 /* 7   */, 0b00 /* 8   */, 0b00 /* 9   */, 0b01 /* :   */, 0b00 /* ;   */, 0b11 /* <   */, 0b00 /* =   */, 0b11 /* >   */, 0b11 /* ?   */,
            0b00 /* @   */, 0b00 /* A   */, 0b00 /* B   */, 0b00 /* C   */, 0b00 /* D   */, 0b00 /* E   */, 0b00 /* F   */, 0b00 /* G   */, 0b00 /* H   */, 0b00 /* I   */, 0b00 /* J   */, 0b00 /* K   */, 0b00 /* L   */, 0b00 /* M   */, 0b00 /* N   */, 0b00 /* O   */,
            0b00 /* P   */, 0b00 /* Q   */, 0b00 /* R   */, 0b00 /* S   */, 0b00 /* T   */, 0b00 /* U   */, 0b00 /* V   */, 0b00 /* W   */, 0b00 /* X   */, 0b00 /* Y   */, 0b00 /* Z   */, 0b00 /* [   */, 0b01 /* \   */, 0b00 /* ]   */, 0b00 /* ^   */, 0b00 /* _   */,
            0b00 /* `   */, 0b00 /* a   */, 0b00 /* b   */, 0b00 /* c   */, 0b00 /* d   */, 0b00 /* e   */, 0b00 /* f   */, 0b00 /* g   */, 0b00 /* h   */, 0b00 /* i   */, 0b00 /* j   */, 0b00 /* k   */, 0b00 /* l   */, 0b00 /* m   */, 0b00 /* n   */, 0b00 /* o   */,
            0b00 /* p   */, 0b00 /* q   */, 0b00 /* r   */, 0b00 /* s   */, 0b00 /* t   */, 0b00 /* u   */, 0b00 /* v   */, 0b00 /* w   */, 0b00 /* x   */, 0b00 /* y   */, 0b00 /* z   */, 0b00 /* {   */, 0b11 /* |   */, 0b00 /* }   */, 0b00 /* ~   */, 0b00 /* DEL */,
            // clang-format on
        } };
    }

    _TIL_INLINEPREFIX std::wstring clean_filename(std::wstring str) noexcept
    {
        std::erase_if(str, [](auto ch) {
            // This lookup is branchless: It always checks the filter, but throws
            // away the result if ch >= 128. This is faster than using `&&` (branchy).
            return til::at(details::pathFilter, ch & 127) & 0b01 & (ch < 128);
        });
        return str;
    }

    _TIL_INLINEPREFIX std::wstring clean_path(std::wstring str) noexcept
    {
        std::erase_if(str, [](auto ch) {
            return ((til::at(details::pathFilter, ch & 127) & 0b10) >> 1) & (ch < 128);
        });
        return str;
    }

    // std::string_view::starts_with support for C++17.
    template<typename T, typename Traits>
    constexpr bool starts_with(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& prefix) noexcept
    {
        return str.size() >= prefix.size() && __builtin_memcmp(str.data(), prefix.data(), prefix.size() * sizeof(T)) == 0;
    }

    constexpr bool starts_with(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return starts_with<>(str, prefix);
    }

    constexpr bool starts_with(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return starts_with<>(str, prefix);
    }

    // std::string_view::ends_with support for C++17.
    template<typename T, typename Traits>
    constexpr bool ends_with(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& suffix) noexcept
    {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        return str.size() >= suffix.size() && __builtin_memcmp(str.data() + (str.size() - suffix.size()), suffix.data(), suffix.size() * sizeof(T)) == 0;
    }

    constexpr bool ends_with(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    constexpr bool ends_with(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    inline constexpr unsigned long to_ulong_error = ULONG_MAX;

    // Just like std::wcstoul, but without annoying locales and null-terminating strings.
    // It has been fuzz-tested against clang's strtoul implementation.
    template<typename T, typename Traits>
    _TIL_INLINEPREFIX constexpr unsigned long to_ulong(const std::basic_string_view<T, Traits>& str, unsigned long base = 0) noexcept
    {
        static constexpr unsigned long maximumValue = ULONG_MAX / 16;

        // We don't have to test ptr for nullability, as we only access it under either condition:
        // * str.length() > 0, for determining the base
        // * ptr != end, when parsing the characters; if ptr is null, length will be 0 and thus end == ptr
#pragma warning(push)
#pragma warning(disable : 26429) // Symbol 'ptr' is never tested for nullness, it can be marked as not_null
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead
        auto ptr = str.data();
        const auto end = ptr + str.length();
        unsigned long accumulator = 0;
        unsigned long value = ULONG_MAX;

        if (!base)
        {
            base = 10;

            if (str.length() > 1 && *ptr == '0')
            {
                base = 8;
                ++ptr;

                if (str.length() > 2 && (*ptr == 'x' || *ptr == 'X'))
                {
                    base = 16;
                    ++ptr;
                }
            }
        }

        if (ptr == end)
        {
            return to_ulong_error;
        }

        for (;; accumulator *= base)
        {
            value = ULONG_MAX;
            if (*ptr >= '0' && *ptr <= '9')
            {
                value = *ptr - '0';
            }
            else if (*ptr >= 'A' && *ptr <= 'F')
            {
                value = *ptr - 'A' + 10;
            }
            else if (*ptr >= 'a' && *ptr <= 'f')
            {
                value = *ptr - 'a' + 10;
            }
            else
            {
                return to_ulong_error;
            }

            accumulator += value;
            if (accumulator >= maximumValue)
            {
                return to_ulong_error;
            }

            if (++ptr == end)
            {
                return accumulator;
            }
        }
#pragma warning(pop)
    }

    constexpr unsigned long to_ulong(const std::string_view& str, unsigned long base = 0) noexcept
    {
        return to_ulong<>(str, base);
    }

    constexpr unsigned long to_ulong(const std::wstring_view& str, unsigned long base = 0) noexcept
    {
        return to_ulong<>(str, base);
    }

    // Just like std::tolower, but without annoying locales.
    template<typename T>
    constexpr T tolower_ascii(T c)
    {
        if ((c >= 'A') && (c <= 'Z'))
        {
            c |= 0x20;
        }

        return c;
    }

    // Just like std::toupper, but without annoying locales.
    template<typename T>
    constexpr T toupper_ascii(T c)
    {
        if ((c >= 'a') && (c <= 'z'))
        {
            c &= ~0x20;
        }

        return c;
    }

    // Just like std::wstring_view::operator==().
    //
    // At the time of writing wmemcmp() is not an intrinsic for MSVC,
    // but the STL uses it to implement wide string comparisons.
    // This produces 3x the assembly _per_ comparison and increases
    // runtime by 2-3x for strings of medium length (16 characters)
    // and 5x or more for long strings (128 characters or more).
    // See: https://github.com/microsoft/STL/issues/2289
    template<typename T, typename Traits>
    bool equals(const std::basic_string_view<T, Traits>& lhs, const std::basic_string_view<T, Traits>& rhs) noexcept
    {
        return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(T)) == 0;
    }

    // Just like _memicmp, but without annoying locales.
    template<typename T, typename Traits>
    bool equals_insensitive_ascii(const std::basic_string_view<T, Traits>& str1, const std::basic_string_view<T, Traits>& str2) noexcept
    {
        if (str1.size() != str2.size())
        {
            return false;
        }

#pragma warning(push)
#pragma warning(disable : 26429) // Symbol 'data1' is never tested for nullness, it can be marked as not_null
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead
        auto remaining = str1.size();
        auto data1 = str1.data();
        auto data2 = str2.data();
        for (; remaining; --remaining, ++data1, ++data2)
        {
            if (*data1 != *data2 && tolower_ascii(*data1) != tolower_ascii(*data2))
            {
                return false;
            }
        }
#pragma warning(pop)

        return true;
    }

    inline bool equals_insensitive_ascii(const std::string_view& str1, const std::string_view& str2) noexcept
    {
        return equals_insensitive_ascii<>(str1, str2);
    }

    inline bool equals_insensitive_ascii(const std::wstring_view& str1, const std::wstring_view& str2) noexcept
    {
        return equals_insensitive_ascii<>(str1, str2);
    }

    template<typename T, typename Traits>
    constexpr bool starts_with_insensitive_ascii(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& prefix) noexcept
    {
        return str.size() >= prefix.size() && equals_insensitive_ascii<>({ str.data(), prefix.size() }, prefix);
    }

    constexpr bool starts_with_insensitive_ascii(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return starts_with_insensitive_ascii<>(str, prefix);
    }

    constexpr bool starts_with_insensitive_ascii(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return starts_with_insensitive_ascii<>(str, prefix);
    }

    template<typename T, typename Traits>
    constexpr bool ends_with_insensitive_ascii(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& suffix) noexcept
    {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        return str.size() >= suffix.size() && equals_insensitive_ascii<>({ str.data() - suffix.size(), suffix.size() }, suffix);
    }

    constexpr bool ends_with_insensitive_ascii(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return ends_with_insensitive_ascii<>(str, prefix);
    }

    constexpr bool ends_with_insensitive_ascii(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    // Give the arguments ("foo bar baz", " "), this method will
    // * modify the first argument to "bar baz"
    // * return "foo"
    // If the needle cannot be found the "str" argument is returned as is.
    template<typename T, typename Traits>
    std::basic_string_view<T, Traits> prefix_split(std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& needle) noexcept
    {
        using view_type = std::basic_string_view<T, Traits>;

        const auto idx = str.find(needle);
        // > If the needle cannot be found the "str" argument is returned as is.
        // ...but if needle is empty, idx will always be npos, forcing us to return str.
        if (idx == view_type::npos || needle.empty())
        {
            return std::exchange(str, {});
        }

        const auto suffixIdx = idx + needle.size();
        const view_type result{ str.data(), idx };
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead
        str = { str.data() + suffixIdx, str.size() - suffixIdx };
        return result;
    }

    inline std::string_view prefix_split(std::string_view& str, const std::string_view& needle) noexcept
    {
        return prefix_split<>(str, needle);
    }

    inline std::wstring_view prefix_split(std::wstring_view& str, const std::wstring_view& needle) noexcept
    {
        return prefix_split<>(str, needle);
    }
}
