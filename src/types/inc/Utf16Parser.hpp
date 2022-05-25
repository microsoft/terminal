/*++
Copyright (c) Microsoft Corporation

Module Name:
- Utf16Parser.hpp

Abstract:
- Parser for grouping together utf16 codepoints from a string of utf16 encoded text

Author(s):
- Austin Diviness (AustDi) 25-Apr-2018

--*/

#pragma once

#include <vector>

class Utf16Parser final
{
public:
    static std::vector<std::vector<wchar_t>> Parse(std::wstring_view wstr);
    static std::wstring_view ParseNext(std::wstring_view wstr) noexcept;

    // Routine Description:
    // - checks if wchar is a utf16 leading surrogate
    // Arguments:
    // - wch - the wchar to check
    // Return Value:
    // - true if wch is a leading surrogate, false otherwise
    static constexpr bool IsLeadingSurrogate(const wchar_t wch) noexcept
    {
        return wch >= 0xD800 && wch <= 0xDBFF;
    }

    // Routine Description:
    // - checks if wchar is a utf16 trailing surrogate
    // Arguments:
    // - wch - the wchar to check
    // Return Value:
    // - true if wch is a trailing surrogate, false otherwise
    static constexpr bool IsTrailingSurrogate(const wchar_t wch) noexcept
    {
        return wch >= 0xDC00 && wch <= 0xDFFF;
    }
};
