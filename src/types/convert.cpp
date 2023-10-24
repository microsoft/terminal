// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/convert.hpp"

#include "../inc/unicode.hpp"

#pragma hdrstop

// Routine Description:
// - Takes a multibyte string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Unicode UTF-16 result in the smart pointer (and the length).
// Arguments:
// - codepage - Windows Code Page representing the multibyte source text
// - source - View of multibyte characters of source text
// Return Value:
// - The UTF-16 wide string.
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::wstring ConvertToW(const UINT codePage, const std::string_view source)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        return {};
    }

    int iSource; // convert to int because Mb2Wc requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // In certain codepages, Mb2Wc will "successfully" produce zero characters (like in CP50220, where a SHIFT-IN character
    // is consumed but not transformed into anything) without explicitly failing. When it does this, GetLastError will return
    // the last error encountered by the last function that actually did have an error.
    // This is arguably correct (as the documentation says "The function returns 0 if it does not succeed"). There is a
    // difference that we **don't actually care about** between failing and successfully producing zero characters.,
    // Anyway: we need to clear the last error so that we can fail out and IGNORE_BAD_GLE after it inevitably succeed-fails.
    SetLastError(0);
    const auto iTarget = MultiByteToWideChar(codePage, 0, source.data(), iSource, nullptr, 0);
    THROW_LAST_ERROR_IF_AND_IGNORE_BAD_GLE(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    std::wstring out;
    out.resize(cchNeeded);

    // Attempt conversion for real.
    THROW_LAST_ERROR_IF_AND_IGNORE_BAD_GLE(0 == MultiByteToWideChar(codePage, 0, source.data(), iSource, out.data(), iTarget));

    // Return as a string
    return out;
}

// Routine Description:
// - Takes a wide string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Multibyte result
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Unicode (UTF-16) characters of source text
// Return Value:
// - The multibyte string encoded in the given codepage
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::string ConvertToA(const UINT codepage, const std::wstring_view source)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        return {};
    }

    int iSource; // convert to int because Wc2Mb requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    const auto iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    std::string out;
    out.resize(cchNeeded);

    // Attempt conversion for real.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    THROW_LAST_ERROR_IF(0 == WideCharToMultiByte(codepage, 0, source.data(), iSource, out.data(), iTarget, nullptr, nullptr));

    // Return as a string
    return out;
}

// Routine Description:
// - Takes a wide string, and determines how many bytes it would take to store it with the given Multibyte codepage.
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Array of Unicode characters of source text
// Return Value:
// - Length in characters of multibyte buffer that would be required to hold this text after conversion
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or WideCharToMultiByte failures.
[[nodiscard]] size_t GetALengthFromW(const UINT codepage, const std::wstring_view source)
{
    // If there's no bytes, bail early.
    if (source.empty())
    {
        return 0;
    }

    int iSource; // convert to int because Wc2Mb requires it
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how many bytes this string consumes in the other codepage
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    const auto iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    // Convert types safely.
    size_t cchTarget;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchTarget));

    return cchTarget;
}

wchar_t Utf16ToUcs2(const std::wstring_view charData)
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    if (charData.size() > 1)
    {
        return UNICODE_REPLACEMENT;
    }
    else
    {
        return charData.front();
    }
}
