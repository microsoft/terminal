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
    enum class AxisTagPresence : BYTE
    {
        None = 0x00,
        Weight = 0x01,
        Width = 0x02,
        Italic = 0x04,
        Slant = 0x08,
    };
    DEFINE_ENUM_FLAG_OPERATORS(AxisTagPresence);

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

        DxFontRenderData(::Microsoft::WRL::ComPtr<IDWriteFactory1> dwriteFactory);

        // DirectWrite text analyzer from the factory
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> Analyzer();

        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFallback> SystemFontFallback();

        // A locale that can be used on construction of assorted DX objects that want to know one.
        [[nodiscard]] std::wstring UserLocaleName();

        [[nodiscard]] til::size GlyphCell() noexcept;
        [[nodiscard]] LineMetrics GetLineMetrics() noexcept;

        // The weight of default font
        [[nodiscard]] DWRITE_FONT_WEIGHT DefaultFontWeight() noexcept;

        // The style of default font
        [[nodiscard]] DWRITE_FONT_STYLE DefaultFontStyle() noexcept;

        // The stretch of default font
        [[nodiscard]] DWRITE_FONT_STRETCH DefaultFontStretch() noexcept;

        // The font features of the default font
        [[nodiscard]] const std::vector<DWRITE_FONT_FEATURE>& DefaultFontFeatures() const noexcept;

        // The DirectWrite format object representing the size and other text properties to be applied (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> DefaultTextFormat();

        // The DirectWrite font face to use while calculating layout (by default)
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DefaultFontFace();

        // Box drawing scaling effects that are cached for the base font across layouts
        [[nodiscard]] Microsoft::WRL::ComPtr<IBoxDrawingEffect> DefaultBoxDrawingEffect();

        // The attributed variants of the format object representing the size and other text properties
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> TextFormatWithAttribute(DWRITE_FONT_WEIGHT weight,
                                                                                        DWRITE_FONT_STYLE style,
                                                                                        DWRITE_FONT_STRETCH stretch);

        // The attributed variants of the font face to use while calculating layout
        [[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> FontFaceWithAttribute(DWRITE_FONT_WEIGHT weight,
                                                                                     DWRITE_FONT_STYLE style,
                                                                                     DWRITE_FONT_STRETCH stretch);

        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& desired, FontInfo& fiFontInfo, const int dpi, const std::unordered_map<std::wstring_view, uint32_t>& features = {}, const std::unordered_map<std::wstring_view, float>& axes = {}) noexcept;

        [[nodiscard]] static HRESULT STDMETHODCALLTYPE s_CalculateBoxEffect(IDWriteTextFormat* format, size_t widthPixels, IDWriteFontFace1* face, float fontScale, IBoxDrawingEffect** effect) noexcept;

        bool DidUserSetFeatures() const noexcept;
        bool DidUserSetAxes() const noexcept;
        void InhibitUserWeight(bool inhibitUserWeight) noexcept;
        bool DidUserSetItalic() const noexcept;

        std::vector<DWRITE_FONT_AXIS_VALUE> GetAxisVector(const DWRITE_FONT_WEIGHT fontWeight,
                                                          const DWRITE_FONT_STRETCH fontStretch,
                                                          const DWRITE_FONT_STYLE fontStyle,
                                                          IDWriteTextFormat3* format);

    private:
        using FontAttributeMapKey = uint32_t;

        bool _inhibitUserWeight{ false };
        bool _didUserSetItalic{ false };
        bool _didUserSetFeatures{ false };
        bool _didUserSetAxes{ false };
        // The font features to apply to the text
        std::vector<DWRITE_FONT_FEATURE> _featureVector;

        // The font axes to apply to the text
        std::vector<DWRITE_FONT_AXIS_VALUE> _axesVector;
        std::span<DWRITE_FONT_AXIS_VALUE> _axesVectorWithoutWeight;

        // We use this to identify font variants with different attributes.
        static FontAttributeMapKey _ToMapKey(DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch) noexcept
        {
            return (weight << 16) | (style << 8) | stretch;
        };

        void _SetFeatures(const std::unordered_map<std::wstring_view, uint32_t>& features);
        void _SetAxes(const std::unordered_map<std::wstring_view, float>& axes);
        float _FontStretchToWidthAxisValue(DWRITE_FONT_STRETCH fontStretch) noexcept;
        float _FontStyleToSlantFixedAxisValue(DWRITE_FONT_STYLE fontStyle) noexcept;
        void _BuildFontRenderData(const FontInfoDesired& desired, FontInfo& actual, const int dpi);
        Microsoft::WRL::ComPtr<IDWriteTextFormat> _BuildTextFormat(const DxFontInfo& fontInfo, const std::wstring_view localeName);

        std::unordered_map<FontAttributeMapKey, ::Microsoft::WRL::ComPtr<IDWriteTextFormat>> _textFormatMap;
        std::unordered_map<FontAttributeMapKey, ::Microsoft::WRL::ComPtr<IDWriteFontFace1>> _fontFaceMap;

        ::Microsoft::WRL::ComPtr<IBoxDrawingEffect> _boxDrawingEffect;
        ::Microsoft::WRL::ComPtr<IDWriteFontFallback> _systemFontFallback;
        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;
        ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _dwriteTextAnalyzer;

        std::wstring _userLocaleName;
        DxFontInfo _defaultFontInfo;
        til::size _glyphCell;
        DWRITE_LINE_SPACING _lineSpacing;
        LineMetrics _lineMetrics;
        float _fontSize;
    };
}
