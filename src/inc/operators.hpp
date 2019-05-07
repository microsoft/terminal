/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- operators.hpp

Abstract:
- This file contains helpful operator overloading for some older data structures.

Author(s):
- Austin Diviness (AustDi) Mar 2017
--*/

#pragma once

constexpr bool operator==(const COORD& a, const COORD& b) noexcept
{
    return (a.X == b.X &&
            a.Y == b.Y);
}

constexpr bool operator!=(const COORD& a, const COORD& b) noexcept
{
    return !(a == b);
}

constexpr bool operator==(const SMALL_RECT& a, const SMALL_RECT& b) noexcept
{
    return (a.Top == b.Top &&
            a.Left == b.Left &&
            a.Bottom == b.Bottom &&
            a.Right == b.Right);
}

constexpr bool operator!=(const SMALL_RECT& a, const SMALL_RECT& b) noexcept
{
    return !(a == b);
}

constexpr bool operator==(const std::wstring& wstr, const std::wstring_view& wstrView)
{
    return (wstrView == std::wstring_view{ wstr.c_str(), wstr.size() });
}

constexpr bool operator==(const std::wstring_view& wstrView, const std::wstring& wstr)
{
    return (wstr == wstrView);
}

constexpr bool operator!=(const std::wstring& wstr, const std::wstring_view& wstrView)
{
    return !(wstr == wstrView);
}

constexpr bool operator!=(const std::wstring_view& wstrView, const std::wstring& wstr)
{
    return !(wstr == wstrView);
}
