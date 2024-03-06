// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "cmdline.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop
using Microsoft::Console::Interactivity::ServiceLocator;

// Routine Description:
// - Detects Word delimiters
bool IsWordDelim(const wchar_t wch) noexcept
{
    // the space character is always a word delimiter. Do not add it to the WordDelimiters global because
    // that contains the user configurable word delimiters only.
    if (wch == UNICODE_SPACE)
    {
        return true;
    }
    const auto& delimiters = ServiceLocator::LocateGlobals().WordDelimiters;
    return std::ranges::find(delimiters, wch) != delimiters.end();
}

bool IsWordDelim(const std::wstring_view& charData) noexcept
{
    return charData.size() == 1 && IsWordDelim(charData.front());
}

// Returns a truthy value for delimiters and 0 otherwise.
// The distinction between whitespace and other delimiters allows us to
// implement Windows' inconsistent, but classic, word-wise navigation.
int DelimiterClass(wchar_t wch) noexcept
{
    if (wch == L' ')
    {
        return 1;
    }
    const auto& delimiters = ServiceLocator::LocateGlobals().WordDelimiters;
    if (std::find(delimiters.begin(), delimiters.end(), wch) != delimiters.end())
    {
        return 2;
    }
    return 0;
}
