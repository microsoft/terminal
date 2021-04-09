// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/FontInfoDesired.hpp"
#include "DxFontInfo.h"
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
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> Analyzer();

        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFallback> SystemFontFallback();

        // A locale that can be used on construction of assorted DX objects that want to know one.
        [[nodiscard]] std::wstring UserLocaleName();

        [[nodiscard]] const Microsoft::WRL::ComPtr<IDWriteFontCollection1>& NearbyCollection() const;

        [[nodiscard]] til::size GlyphCell() noexcept;
        [[nodiscard]] LineMetrics GetLineMetrics() noexcept;

        // The DirectWrite format object representing the size and other text properties to be applied (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> DefaultTextFormat();

        // The DirectWrite font face to use while calculating layout (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DefaultFontFace();

        // Box drawing scaling effects that are cached for the base font across layouts
        [[nodiscard]] Microsoft::WRL::ComPtr<IBoxDrawingEffect> DefaultBoxDrawingEffect();

        // The italic variant of the format object representing the size and other text properties for italic text
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> ItalicTextFormat();

        // The italic variant of the font face to use while calculating layout for italic text
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> ItalicFontFace();

        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& desired, FontInfo& fiFontInfo, const int dpi) noexcept;

        [[nodiscard]] static HRESULT STDMETHODCALLTYPE s_CalculateBoxEffect(IDWriteTextFormat* format, size_t widthPixels, IDWriteFontFace1* face, float fontScale, IBoxDrawingEffect** effect) noexcept;

    private:
        void _BuildFontRenderData(const FontInfoDesired& desired, FontInfo& actual, const int dpi);
        Microsoft::WRL::ComPtr<IDWriteTextFormat> _BuildTextFormat(const DxFontInfo fontInfo, const std::wstring_view localeName);

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
        DxFontInfo _defaultFontInfo;

        float _fontSize;
        til::size _glyphCell;
        DWRITE_LINE_SPACING _lineSpacing;
        LineMetrics _lineMetrics;
    };
}
