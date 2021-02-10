// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/IFontRenderData.hpp"
#include "../../renderer/inc/FontInfoDesired.hpp"

#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <wrl.h>

namespace Microsoft::Console::Render
{
    struct LineMetrics
    {
        float gridlineWidth;
        float underlineOffset;
        float underlineOffset2;
        float underlineWidth;
        float strikethroughOffset;
        float strikethroughWidth;
    };

    class DxFontRenderData: IFontRenderData<Microsoft::WRL::ComPtr<IDWriteTextFormat>>
    {
    public:
        DxFontRenderData(::Microsoft::WRL::ComPtr<IDWriteFactory1> dwriteFactory);

        Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> Analyzer();

        Microsoft::WRL::ComPtr<IDWriteTextFormat> Regular();
        Microsoft::WRL::ComPtr<IDWriteFontFace1> RegularFontFace();

        Microsoft::WRL::ComPtr<IDWriteTextFormat> Italic();
        Microsoft::WRL::ComPtr<IDWriteFontFace1> ItalicFontFace();

        HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo, const int dpi) noexcept;

    private:
        [[nodiscard]] HRESULT _GetProposedFont(const FontInfoDesired& desired,
                                               FontInfo& actual,
                                               const int dpi,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormat,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormatItalic,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1>& textAnalyzer,
                                               ::Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFace,
                                               ::Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFaceItalic,
                                               LineMetrics& lineMetrics) const noexcept;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                              DWRITE_FONT_WEIGHT& weight,
                                                                                              DWRITE_FONT_STRETCH& stretch,
                                                                                              DWRITE_FONT_STYLE& style,
                                                                                              std::wstring& localeName) const;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(std::wstring& familyName,
                                                                               DWRITE_FONT_WEIGHT& weight,
                                                                               DWRITE_FONT_STRETCH& stretch,
                                                                               DWRITE_FONT_STYLE& style,
                                                                               std::wstring& localeName) const;

        [[nodiscard]] std::wstring _GetLocaleName() const;

        [[nodiscard]] std::wstring _GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                      std::wstring& localeName) const;

        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;

        ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _dwriteTextAnalyzer;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormat;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormatItalic;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFace;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFaceItalic;

        LineMetrics _lineMetrics;
    };
}
