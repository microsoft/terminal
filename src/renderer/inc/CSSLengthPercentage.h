// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

struct CSSLengthPercentage
{
    enum class ReferenceFrame : uint8_t
    {
        // This indicates the object is empty/unset.
        // No need to call Resolve().
        None,
        // Call Resolve() with factor set to the target DPI (e.g. 96 for DIP).
        // Returns an absolute length value scaled by that DPI.
        Absolute,
        // Call Resolve() with factor set to the font size
        // in an arbitrary DPI. Returns a value relative to it.
        FontSize,
        // Call Resolve() with factor set to the "0" glyph advance width
        // in an arbitrary DPI. Returns a value relative to it.
        AdvanceWidth,
    };

    static CSSLengthPercentage FromString(const wchar_t* str);

    float Resolve(float fallback, float dpi, float fontSize, float advanceWidth) const noexcept;

private:
    float _value = 0;
    ReferenceFrame _referenceFrame = ReferenceFrame::None;
};
