// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/FontInfoDesired.hpp"
#include "BoxDrawingEffect.h"

#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <wrl.h>

namespace Microsoft::Console::Render
{
    class DxFontRenderData
    {
    public:
        struct LineMetrics
        {
            float gridlineWidth;
            float underlineOffset;
            float underlineOffset2;
            float underlineWidth;
            float strikethroughOffset;
            float strikethroughWidth;
        };

        DxFontRenderData(::Microsoft::WRL::ComPtr<IDWriteFactory1> dwriteFactory) noexcept;

        // DirectWrite text analyzer from the factory
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> Analyzer() noexcept;

        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFallback> SystemFontFallback();

        [[nodiscard]] const Microsoft::WRL::ComPtr<IDWriteFontCollection1>& NearbyCollection() const;

        [[nodiscard]] til::size GlyphCell() noexcept;
        [[nodiscard]] LineMetrics GetLineMetrics() noexcept;

        // The DirectWrite format object representing the size and other text properties to be applied (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> DefaultTextFormat() noexcept;

        // The DirectWrite font face to use while calculating layout (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DefaultFontFace() noexcept;

        // Box drawing scaling effects that are cached for the base font across layouts
        [[nodiscard]] Microsoft::WRL::ComPtr<IBoxDrawingEffect> DefaultBoxDrawingEffect() noexcept;

        // The italic variant of the format object representing the size and other text properties for italic text
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> ItalicTextFormat() noexcept;

        // The italic variant of the font face to use while calculating layout for italic text
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> ItalicFontFace() noexcept;

        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& desired, FontInfo& fiFontInfo, const int dpi) noexcept;

        [[nodiscard]] static HRESULT STDMETHODCALLTYPE s_CalculateBoxEffect(IDWriteTextFormat* format, size_t widthPixels, IDWriteFontFace1* face, float fontScale, IBoxDrawingEffect** effect) noexcept;

    private:
        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                              DWRITE_FONT_WEIGHT& weight,
                                                                                              DWRITE_FONT_STRETCH& stretch,
                                                                                              DWRITE_FONT_STYLE& style,
                                                                                              std::wstring& localeName,
                                                                                              bool& didFallback) const;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(std::wstring& familyName,
                                                                               DWRITE_FONT_WEIGHT& weight,
                                                                               DWRITE_FONT_STRETCH& stretch,
                                                                               DWRITE_FONT_STYLE& style,
                                                                               std::wstring& localeName) const;

        [[nodiscard]] std::wstring _GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                      std::wstring& localeName) const;

        // A locale that can be used on construction of assorted DX objects that want to know one.
        [[nodiscard]] std::wstring _GetUserLocaleName();

        [[nodiscard]] static std::vector<std::filesystem::path> s_GetNearbyFonts();

        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;

        ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _dwriteTextAnalyzer;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormat;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormatItalic;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFace;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFaceItalic;

        ::Microsoft::WRL::ComPtr<IBoxDrawingEffect> _boxDrawingEffect;

        ::Microsoft::WRL::ComPtr<IDWriteFontFallback> _systemFontFallback;
        mutable ::Microsoft::WRL::ComPtr<IDWriteFontCollection1> _nearbyCollection;
        std::wstring _userLocaleName;

        til::size _glyphCell;

        LineMetrics _lineMetrics;
    };
}
