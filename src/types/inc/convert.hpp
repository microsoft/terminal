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
#include <deque>
#include <string>
#include <string_view>
#include <memory>
#include "IInputEvent.hpp"

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

std::deque<std::unique_ptr<KeyEvent>> CharToKeyEvents(const wchar_t wch, const unsigned int codepage);

std::deque<std::unique_ptr<KeyEvent>> SynthesizeKeyboardEvents(const wchar_t wch,
                                                               const short keyState);

std::deque<std::unique_ptr<KeyEvent>> SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage);

CodepointWidth GetQuickCharWidth(const wchar_t wch) noexcept;

wchar_t Utf16ToUcs2(const std::wstring_view charData);
