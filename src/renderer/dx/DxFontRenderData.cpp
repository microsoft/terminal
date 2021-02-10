// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DxFontRenderData.h"

static constexpr float POINTS_PER_INCH = 72.0f;
static constexpr std::wstring_view FALLBACK_FONT_FACES[] = { L"Consolas", L"Lucida Console", L"Courier New" };
static constexpr std::wstring_view FALLBACK_LOCALE = L"en-us";

using namespace Microsoft::Console::Render;

DxFontRenderData::DxFontRenderData(::Microsoft::WRL::ComPtr<IDWriteFactory1> dwriteFactory):
    _dwriteFactory(dwriteFactory),
    _lineMetrics({})
{
}

Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> DxFontRenderData::Analyzer()
{
    return _dwriteTextAnalyzer;
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> DxFontRenderData::Regular()
{
    return _dwriteTextFormat;
}

Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::RegularFontFace()
{
    return _dwriteFontFace;
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> DxFontRenderData::Italic()
{
    return _dwriteTextFormatItalic;
}

Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::ItalicFontFace()
{
    return _dwriteFontFaceItalic;
}

HRESULT DxFontRenderData::UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo,
                                     const int dpi) noexcept
{
    RETURN_IF_FAILED(_GetProposedFont(pfiFontInfoDesired,
                                      fiFontInfo,
                                      dpi,
                                      _dwriteTextFormat,
                                      _dwriteTextFormatItalic,
                                      _dwriteTextAnalyzer,
                                      _dwriteFontFace,
                                      _dwriteFontFaceItalic,
                                      _lineMetrics));
}


// Routine Description:
// - Updates the font used for drawing
// Arguments:
// - desired - Information specifying the font that is requested
// - actual - Filled with the nearest font actually chosen for drawing
// - dpi - The DPI of the screen
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxFontRenderData::_GetProposedFont(const FontInfoDesired& desired,
                                                         FontInfo& actual,
                                                         const int dpi,
                                                         Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormat,
                                                         Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormatItalic,
                                                         Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1>& textAnalyzer,
                                                         Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFace,
                                                         Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFaceItalic,
                                                         LineMetrics& lineMetrics) const noexcept
        {
    try
    {
        std::wstring fontName(desired.GetFaceName());
        DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(desired.GetWeight());
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
        std::wstring localeName = _GetLocaleName();

        // _ResolveFontFaceWithFallback overrides the last argument with the locale name of the font,
        // but we should use the system's locale to render the text.
        std::wstring fontLocaleName = localeName;
        const auto face = _ResolveFontFaceWithFallback(fontName, weight, stretch, style, fontLocaleName);

        DWRITE_FONT_METRICS1 fontMetrics;
        face->GetMetrics(&fontMetrics);

        const UINT32 spaceCodePoint = L'M';
        UINT16 spaceGlyphIndex;
        THROW_IF_FAILED(face->GetGlyphIndicesW(&spaceCodePoint, 1, &spaceGlyphIndex));

        INT32 advanceInDesignUnits;
        THROW_IF_FAILED(face->GetDesignGlyphAdvances(1, &spaceGlyphIndex, &advanceInDesignUnits));

        DWRITE_GLYPH_METRICS spaceMetrics = { 0 };
        THROW_IF_FAILED(face->GetDesignGlyphMetrics(&spaceGlyphIndex, 1, &spaceMetrics));

        // The math here is actually:
        // Requested Size in Points * DPI scaling factor * Points to Pixels scaling factor.
        // - DPI = dots per inch
        // - PPI = points per inch or "points" as usually seen when choosing a font size
        // - The DPI scaling factor is the current monitor DPI divided by 96, the default DPI.
        // - The Points to Pixels factor is based on the typography definition of 72 points per inch.
        //    As such, converting requires taking the 96 pixel per inch default and dividing by the 72 points per inch
        //    to get a factor of 1 and 1/3.
        // This turns into something like:
        // - 12 ppi font * (96 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 16 pixels tall font for 100% display (96 dpi is 100%)
        // - 12 ppi font * (144 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 24 pixels tall font for 150% display (144 dpi is 150%)
        // - 12 ppi font * (192 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 32 pixels tall font for 200% display (192 dpi is 200%)
        float heightDesired = static_cast<float>(desired.GetEngineSize().Y) * static_cast<float>(USER_DEFAULT_SCREEN_DPI) / POINTS_PER_INCH;

        // The advance is the number of pixels left-to-right (X dimension) for the given font.
        // We're finding a proportional factor here with the design units in "ems", not an actual pixel measurement.

        // Now we play trickery with the font size. Scale by the DPI to get the height we expect.
        heightDesired *= (static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI));

        const float widthAdvance = static_cast<float>(advanceInDesignUnits) / fontMetrics.designUnitsPerEm;

        // Use the real pixel height desired by the "em" factor for the width to get the number of pixels
        // we will need per character in width. This will almost certainly result in fractional X-dimension pixels.
        const float widthApprox = heightDesired * widthAdvance;

        // Since we can't deal with columns of the presentation grid being fractional pixels in width, round to the nearest whole pixel.
        const float widthExact = round(widthApprox);

        // Now reverse the "em" factor from above to turn the exact pixel width into a (probably) fractional
        // height in pixels of each character. It's easier for us to pad out height and align vertically
        // than it is horizontally.
        const auto fontSize = widthExact / widthAdvance;

        // Now figure out the basic properties of the character height which include ascent and descent
        // for this specific font size.
        const float ascent = (fontSize * fontMetrics.ascent) / fontMetrics.designUnitsPerEm;
        const float descent = (fontSize * fontMetrics.descent) / fontMetrics.designUnitsPerEm;

        // Get the gap.
        const float gap = (fontSize * fontMetrics.lineGap) / fontMetrics.designUnitsPerEm;
        const float halfGap = gap / 2;

        // We're going to build a line spacing object here to track all of this data in our format.
        DWRITE_LINE_SPACING lineSpacing = {};
        lineSpacing.method = DWRITE_LINE_SPACING_METHOD_UNIFORM;

        // We need to make sure the baseline falls on a round pixel (not a fractional pixel).
        // If the baseline is fractional, the text appears blurry, especially at small scales.
        // Since we also need to make sure the bounding box as a whole is round pixels
        // (because the entire console system maths in full cell units),
        // we're just going to ceiling up the ascent and descent to make a full pixel amount
        // and set the baseline to the full round pixel ascent value.
        //
        // For reference, for the letters "ag":
        // ...
        //          gggggg      bottom of previous line
        //
        // -----------------    <===========================================|
        //                         | topSideBearing       |  1/2 lineGap    |
        // aaaaaa   ggggggg     <-------------------------|-------------|   |
        //      a   g    g                                |             |   |
        //  aaaaa   ggggg                                 |<-ascent     |   |
        // a    a   g                                     |             |   |---- lineHeight
        // aaaaa a  gggggg      <----baseline, verticalOriginY----------|---|
        //          g     g                               |<-descent    |   |
        //          gggggg      <-------------------------|-------------|   |
        //                         | bottomSideBearing    | 1/2 lineGap     |
        // -----------------    <===========================================|
        //
        // aaaaaa   ggggggg     top of next line
        // ...
        //
        // Also note...
        // We're going to add half the line gap to the ascent and half the line gap to the descent
        // to ensure that the spacing is balanced vertically.
        // Generally speaking, the line gap is added to the ascent by DirectWrite itself for
        // horizontally drawn text which can place the baseline and glyphs "lower" in the drawing
        // box than would be desired for proper alignment of things like line and box characters
        // which will try to sit centered in the area and touch perfectly with their neighbors.

        const auto fullPixelAscent = ceil(ascent + halfGap);
        const auto fullPixelDescent = ceil(descent + halfGap);
        lineSpacing.height = fullPixelAscent + fullPixelDescent;
        lineSpacing.baseline = fullPixelAscent;

        // According to MSDN (https://docs.microsoft.com/en-us/windows/win32/api/dwrite_3/ne-dwrite_3-dwrite_font_line_gap_usage)
        // Setting "ENABLED" means we've included the line gapping in the spacing numbers given.
        lineSpacing.fontLineGapUsage = DWRITE_FONT_LINE_GAP_USAGE_ENABLED;

        // Create the font with the fractional pixel height size.
        // It should have an integer pixel width by our math above.
        // Then below, apply the line spacing to the format to position the floating point pixel height characters
        // into a cell that has an integer pixel height leaving some padding above/below as necessary to round them out.
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        THROW_IF_FAILED(_dwriteFactory->CreateTextFormat(fontName.data(),
                                                         nullptr,
                                                         weight,
                                                         style,
                                                         stretch,
                                                         fontSize,
                                                         localeName.data(),
                                                         &format));

        THROW_IF_FAILED(format.As(&textFormat));

        // We also need to create an italic variant of the font face and text
        // format, based on the same parameters, but using an italic style.
        std::wstring fontNameItalic = fontName;
        DWRITE_FONT_WEIGHT weightItalic = weight;
        DWRITE_FONT_STYLE styleItalic = DWRITE_FONT_STYLE_ITALIC;
        DWRITE_FONT_STRETCH stretchItalic = stretch;

        const auto faceItalic = _ResolveFontFaceWithFallback(fontNameItalic, weightItalic, stretchItalic, styleItalic, fontLocaleName);

        Microsoft::WRL::ComPtr<IDWriteTextFormat> formatItalic;
        THROW_IF_FAILED(_dwriteFactory->CreateTextFormat(fontNameItalic.data(),
                                                         nullptr,
                                                         weightItalic,
                                                         styleItalic,
                                                         stretchItalic,
                                                         fontSize,
                                                         localeName.data(),
                                                         &formatItalic));

        THROW_IF_FAILED(formatItalic.As(&textFormatItalic));

        Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer;
        THROW_IF_FAILED(_dwriteFactory->CreateTextAnalyzer(&analyzer));
        THROW_IF_FAILED(analyzer.As(&textAnalyzer));

        fontFace = face;
        fontFaceItalic = faceItalic;

        THROW_IF_FAILED(textFormat->SetLineSpacing(lineSpacing.method, lineSpacing.height, lineSpacing.baseline));
        THROW_IF_FAILED(textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        THROW_IF_FAILED(textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

        // The scaled size needs to represent the pixel box that each character will fit within for the purposes
        // of hit testing math and other such multiplication/division.
        COORD coordSize = { 0 };
        coordSize.X = gsl::narrow<SHORT>(widthExact);
        coordSize.Y = gsl::narrow_cast<SHORT>(lineSpacing.height);

        // Unscaled is for the purposes of re-communicating this font back to the renderer again later.
        // As such, we need to give the same original size parameter back here without padding
        // or rounding or scaling manipulation.
        const COORD unscaled = desired.GetEngineSize();

        const COORD scaled = coordSize;

        actual.SetFromEngine(fontName,
                             desired.GetFamily(),
                             textFormat->GetFontWeight(),
                             false,
                             scaled,
                             unscaled);

        // There is no font metric for the grid line width, so we use a small
        // multiple of the font size, which typically rounds to a pixel.
        lineMetrics.gridlineWidth = std::round(fontSize * 0.025f);

        // All other line metrics are in design units, so to get a pixel value,
        // we scale by the font size divided by the design-units-per-em.
        const auto scale = fontSize / fontMetrics.designUnitsPerEm;
        lineMetrics.underlineOffset = std::round(fontMetrics.underlinePosition * scale);
        lineMetrics.underlineWidth = std::round(fontMetrics.underlineThickness * scale);
        lineMetrics.strikethroughOffset = std::round(fontMetrics.strikethroughPosition * scale);
        lineMetrics.strikethroughWidth = std::round(fontMetrics.strikethroughThickness * scale);

        // We always want the lines to be visible, so if a stroke width ends up
        // at zero after rounding, we need to make it at least 1 pixel.
        lineMetrics.gridlineWidth = std::max(lineMetrics.gridlineWidth, 1.0f);
        lineMetrics.underlineWidth = std::max(lineMetrics.underlineWidth, 1.0f);
        lineMetrics.strikethroughWidth = std::max(lineMetrics.strikethroughWidth, 1.0f);

        // Offsets are relative to the base line of the font, so we subtract
        // from the ascent to get an offset relative to the top of the cell.
        lineMetrics.underlineOffset = fullPixelAscent - lineMetrics.underlineOffset;
        lineMetrics.strikethroughOffset = fullPixelAscent - lineMetrics.strikethroughOffset;

        // For double underlines we need a second offset, just below the first,
        // but with a bit of a gap (about double the grid line width).
        lineMetrics.underlineOffset2 = lineMetrics.underlineOffset +
                                       lineMetrics.underlineWidth +
                                       std::round(fontSize * 0.05f);

        // However, we don't want the underline to extend past the bottom of the
        // cell, so we clamp the offset to fit just inside.
        const auto maxUnderlineOffset = lineSpacing.height - _lineMetrics.underlineWidth;
        lineMetrics.underlineOffset2 = std::min(lineMetrics.underlineOffset2, maxUnderlineOffset);

        // But if the resulting gap isn't big enough even to register as a thicker
        // line, it's better to place the second line slightly above the first.
        if (lineMetrics.underlineOffset2 < lineMetrics.underlineOffset + lineMetrics.gridlineWidth)
        {
            lineMetrics.underlineOffset2 = lineMetrics.underlineOffset - lineMetrics.gridlineWidth;
        }

        // We also add half the stroke width to the offsets, since the line
        // coordinates designate the center of the line.
        lineMetrics.underlineOffset += lineMetrics.underlineWidth / 2.0f;
        lineMetrics.underlineOffset2 += lineMetrics.underlineWidth / 2.0f;
        lineMetrics.strikethroughOffset += lineMetrics.strikethroughWidth / 2.0f;
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Attempts to locate the font given, but then begins falling back if we cannot find it.
// - We'll try to fall back to Consolas with the given weight/stretch/style first,
//   then try Consolas again with normal weight/stretch/style,
//   and if nothing works, then we'll throw an error.
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::_ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                                      DWRITE_FONT_WEIGHT& weight,
                                                                                                      DWRITE_FONT_STRETCH& stretch,
                                                                                                      DWRITE_FONT_STYLE& style,
                                                                                                      std::wstring& localeName) const
{
    auto face = _FindFontFace(familyName, weight, stretch, style, localeName);

    if (!face)
    {
        for (const auto fallbackFace : FALLBACK_FONT_FACES)
        {
            familyName = fallbackFace;
            face = _FindFontFace(familyName, weight, stretch, style, localeName);

            if (face)
            {
                break;
            }

            familyName = fallbackFace;
            weight = DWRITE_FONT_WEIGHT_NORMAL;
            stretch = DWRITE_FONT_STRETCH_NORMAL;
            style = DWRITE_FONT_STYLE_NORMAL;
            face = _FindFontFace(familyName, weight, stretch, style, localeName);

            if (face)
            {
                break;
            }
        }
    }

    THROW_HR_IF_NULL(E_FAIL, face);

    return face;
}

// Routine Description:
// - Locates a suitable font face from the given information
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::_FindFontFace(std::wstring& familyName,
                                                                                       DWRITE_FONT_WEIGHT& weight,
                                                                                       DWRITE_FONT_STRETCH& stretch,
                                                                                       DWRITE_FONT_STYLE& style,
                                                                                       std::wstring& localeName) const
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> fontFace;

    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
    THROW_IF_FAILED(_dwriteFactory->GetSystemFontCollection(&fontCollection, false));

    UINT32 familyIndex;
    BOOL familyExists;
    THROW_IF_FAILED(fontCollection->FindFamilyName(familyName.data(), &familyIndex, &familyExists));

    if (familyExists)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        THROW_IF_FAILED(fontCollection->GetFontFamily(familyIndex, &fontFamily));

        Microsoft::WRL::ComPtr<IDWriteFont> font;
        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(weight, stretch, style, &font));

        Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace0;
        THROW_IF_FAILED(font->CreateFontFace(&fontFace0));

        THROW_IF_FAILED(fontFace0.As(&fontFace));

        // Retrieve metrics in case the font we created was different than what was requested.
        weight = font->GetWeight();
        stretch = font->GetStretch();
        style = font->GetStyle();

        // Dig the family name out at the end to return it.
        familyName = _GetFontFamilyName(fontFamily.Get(), localeName);
    }

    return fontFace;
}

// Routine Description:
// - Helper to retrieve the user's locale preference or fallback to the default.
// Arguments:
// - <none>
// Return Value:
// - A locale that can be used on construction of assorted DX objects that want to know one.
[[nodiscard]] std::wstring DxFontRenderData::_GetLocaleName() const
{
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> localeName;

    const auto returnCode = GetUserDefaultLocaleName(localeName.data(), gsl::narrow<int>(localeName.size()));
    if (returnCode)
    {
        return { localeName.data() };
    }
    else
    {
        return { FALLBACK_LOCALE.data(), FALLBACK_LOCALE.size() };
    }
}

// Routine Description:
// - Retrieves the font family name out of the given object in the given locale.
// - If we can't find a valid name for the given locale, we'll fallback and report it back.
// Arguments:
// - fontFamily - DirectWrite font family object
// - localeName - The locale in which the name should be retrieved.
//              - If fallback occurred, this is updated to what we retrieved instead.
// Return Value:
// - Localized string name of the font family
[[nodiscard]] std::wstring DxFontRenderData::_GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                                std::wstring& localeName) const
{
    // See: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefontcollection
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
    THROW_IF_FAILED(fontFamily->GetFamilyNames(&familyNames));

    // First we have to find the right family name for the locale. We're going to bias toward what the caller
    // requested, but fallback if we need to and reply with the locale we ended up choosing.
    UINT32 index = 0;
    BOOL exists = false;

    // This returns S_OK whether or not it finds a locale name. Check exists field instead.
    // If it returns an error, it's a real problem, not an absence of this locale name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-findlocalename
    THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));

    // If we tried and it still doesn't exist, try with the fallback locale.
    if (!exists)
    {
        localeName = FALLBACK_LOCALE;
        THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));
    }

    // If it still doesn't exist, we're going to try index 0.
    if (!exists)
    {
        index = 0;

        // Get the locale name out so at least the caller knows what locale this name goes with.
        UINT32 length = 0;
        THROW_IF_FAILED(familyNames->GetLocaleNameLength(index, &length));
        localeName.resize(length);

        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalenamelength
        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalename
        // GetLocaleNameLength does not include space for null terminator, but GetLocaleName needs it so add one.
        THROW_IF_FAILED(familyNames->GetLocaleName(index, localeName.data(), length + 1));
    }

    // OK, now that we've decided which family name and the locale that it's in... let's go get it.
    UINT32 length = 0;
    THROW_IF_FAILED(familyNames->GetStringLength(index, &length));

    // Make our output buffer and resize it so it is allocated.
    std::wstring retVal;
    retVal.resize(length);

    // FINALLY, go fetch the string name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstringlength
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstring
    // Once again, GetStringLength is without the null, but GetString needs the null. So add one.
    THROW_IF_FAILED(familyNames->GetString(index, retVal.data(), length + 1));

    // and return it.
    return retVal;
}
