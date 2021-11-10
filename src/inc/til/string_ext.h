// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// At the time of writing wmemcmp() is not an intrinsic for MSVC,
// but the STL uses it to implement wide string comparisons.
// This produces 3x the assembly _per_ comparison and increases
// runtime by 2-3x for strings of medium length (16 characters)
// and 5x or more for long strings (128 characters or more).
// See: https://github.com/microsoft/STL/issues/2289
//
// This reduces the binary size of conhost by about 0.25% (or 3kB).
// It has no performance advantage in the general case, but
// trivially prevents us from running into one in edge cases.
constexpr bool operator==(const std::wstring_view& lhs, const std::wstring_view& rhs) noexcept
{
    return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) == 0;
}

_CONSTEXPR20 bool operator==(const std::wstring_view& lhs, const std::wstring& rhs) noexcept
{
    return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) == 0;
}

_CONSTEXPR20 bool operator==(const std::wstring& rhs, const std::wstring_view& lhs) noexcept
{
    return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) == 0;
}

_CONSTEXPR20 bool operator==(const std::wstring& lhs, const std::wstring& rhs) noexcept
{
    return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) == 0;
}

constexpr bool operator==(const std::wstring_view& lhs, const wchar_t* rhs) noexcept
{
    return lhs == std::wstring_view{ rhs };
}

constexpr bool operator==(const wchar_t* lhs, std::wstring_view& rhs) noexcept
{
    return std::wstring_view{ lhs } == rhs;
}

_CONSTEXPR20 bool operator==(const std::wstring& lhs, const wchar_t* rhs) noexcept
{
    return lhs == std::wstring_view{ rhs };
}

_CONSTEXPR20 bool operator==(const wchar_t* lhs, std::wstring& rhs) noexcept
{
    return std::wstring_view{ lhs } == rhs;
}

constexpr bool operator!=(const std::wstring_view& lhs, const std::wstring_view& rhs) noexcept
{
    return lhs.size() != rhs.size() || __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) != 0;
}

_CONSTEXPR20 bool operator!=(const std::wstring_view& lhs, const std::wstring& rhs) noexcept
{
    return lhs.size() != rhs.size() || __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) != 0;
}

_CONSTEXPR20 bool operator!=(const std::wstring& rhs, const std::wstring_view& lhs) noexcept
{
    return lhs.size() != rhs.size() || __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) != 0;
}

_CONSTEXPR20 bool operator!=(const std::wstring& lhs, const std::wstring& rhs) noexcept
{
    return lhs.size() != rhs.size() || __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(wchar_t)) != 0;
}

constexpr bool operator!=(const std::wstring_view& lhs, const wchar_t* rhs) noexcept
{
    return lhs != std::wstring_view{ rhs };
}

constexpr bool operator!=(const wchar_t* lhs, std::wstring_view& rhs) noexcept
{
    return std::wstring_view{ lhs } != rhs;
}

_CONSTEXPR20 bool operator!=(const std::wstring& lhs, const wchar_t* rhs) noexcept
{
    return lhs != std::wstring_view{ rhs };
}

_CONSTEXPR20 bool operator!=(const wchar_t* lhs, std::wstring& rhs) noexcept
{
    return std::wstring_view{ lhs } != rhs;
}
