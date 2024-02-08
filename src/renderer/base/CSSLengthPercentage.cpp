// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/CSSLengthPercentage.h"

CSSLengthPercentage CSSLengthPercentage::FromString(const wchar_t* str)
{
    auto& errnoRef = errno; // Nonzero cost, pay it once.
    errnoRef = 0;

    wchar_t* end;
    auto value = std::wcstof(str, &end);

    if (str == end || errnoRef == ERANGE)
    {
        return {};
    }

    auto referenceFrame = ReferenceFrame::FontSize;

    if (*end)
    {
        if (wcscmp(end, L"%") == 0)
        {
            value /= 100.0f;
        }
        else if (wcscmp(end, L"px") == 0)
        {
            referenceFrame = ReferenceFrame::Absolute;
            value /= 96.0f;
        }
        else if (wcscmp(end, L"pt") == 0)
        {
            referenceFrame = ReferenceFrame::Absolute;
            value /= 72.0f;
        }
        else if (wcscmp(end, L"ch") == 0)
        {
            referenceFrame = ReferenceFrame::AdvanceWidth;
        }
        else
        {
            return {};
        }
    }

    CSSLengthPercentage obj;
    obj._value = value;
    obj._referenceFrame = referenceFrame;
    return obj;
}

float CSSLengthPercentage::Resolve(float fallback, float dpi, float fontSize, float advanceWidth) const noexcept
{
    switch (_referenceFrame)
    {
    case ReferenceFrame::Absolute:
        return _value * dpi;
    case ReferenceFrame::FontSize:
        return _value * fontSize;
    case ReferenceFrame::AdvanceWidth:
        return _value * advanceWidth;
    default:
        return fallback;
    }
}
