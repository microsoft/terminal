// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

struct CSSLengthPercentage
{
    enum class ReferenceFrame : uint8_t
    {
        // This indicates the object is empty/unset.
        // No need to call Resolve().
        //
        // _value will be 0.
        None,
        // Call Resolve() with factor set to the target DPI (e.g. 96 for DIP).
        // Returns an absolute length value scaled by that DPI.
        //
        // Inputs with a "pt" or "px" suffix are considered "absolute".
        // _value contains an absolute size in CSS inches. In other words,
        // an input of "96px" or "72pt" results in a _value of 1.
        Absolute,
        // Call Resolve() with factor set to the font size
        // in an arbitrary DPI. Returns a value relative to it.
        //
        // Inputs with no suffix or "%" are considered font-size dependent.
        // _value should be multiplied by the current font-size to get the new font-size.
        FontSize,
        // Call Resolve() with factor set to the "0" glyph advance width
        // in an arbitrary DPI. Returns a value relative to it.
        //
        // Inputs with a "ch" suffix are considered advance-width dependent.
        // _value should be multiplied by the current advance-width to get the new font-size.
        AdvanceWidth,
    };

    static CSSLengthPercentage FromString(const wchar_t* str) noexcept;
    static constexpr CSSLengthPercentage FromPixel(float px) noexcept
    {
        CSSLengthPercentage obj;
        obj._value = px / 96.0f;
        obj._referenceFrame = ReferenceFrame::Absolute;
        return obj;
    }

    bool operator==(const CSSLengthPercentage& other) const noexcept;

    ReferenceFrame Reference() const noexcept;
    float Resolve(float fallback, float dpi, float fontSize, float advanceWidth) const noexcept;

private:
    float _value = 0;
    ReferenceFrame _referenceFrame = ReferenceFrame::None;
};
