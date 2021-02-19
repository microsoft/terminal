/*++
Copyright (c) Microsoft Corporation

Module Name:
- convert.hpp

Abstract:
- Defines functions for converting between A and W text strings.
    Largely taken from misc.h.

Author:
- Mike Griese (migrie) 30-Oct-2017

--*/

#pragma once
#include <string>
#include <string_view>

enum class CodepointWidth : BYTE
{
    Narrow,
    Wide,
    Ambiguous, // could be narrow or wide depending on the current codepage and font
    Invalid // not a valid unicode codepoint
};

[[nodiscard]] std::wstring ConvertToW(const UINT codepage,
                                      const std::string_view source);

[[nodiscard]] std::string ConvertToA(const UINT codepage,
                                     const std::wstring_view source);

[[nodiscard]] size_t GetALengthFromW(const UINT codepage,
                                     const std::wstring_view source);

CodepointWidth GetQuickCharWidth(const wchar_t wch) noexcept;

wchar_t Utf16ToUcs2(const std::wstring_view charData);
