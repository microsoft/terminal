// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "Backend.h"
#include "../base/FontCache.h"

// #### NOTE ####
// If you see any code in here that contains "_r." you might be seeing a race condition.
// The AtlasEngine::Present() method is called on a background thread without any locks,
// while any of the API methods (like AtlasEngine::Invalidate) might be called concurrently.
// The usage of _r fields is unsafe as those are accessed and written to by the Present() method.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

// Like gsl::narrow but returns a HRESULT.
#pragma warning(push)
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
template<typename T, typename U>
constexpr HRESULT api_narrow(U val, T& out) noexcept
{
    out = static_cast<T>(val);
    return static_cast<U>(out) != val || (std::is_signed_v<T> != std::is_signed_v<U> && out < T{} != val < U{}) ? HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) : S_OK;
}
#pragma warning(pop)

template<typename T, typename U>
constexpr HRESULT vec2_narrow(U x, U y, vec2<T>& out) noexcept
{
    return api_narrow(x, out.x) | api_narrow(y, out.y);
}

void AtlasEngine::_updateFont(const wchar_t* faceName, const FontSettings& fontSettings)
{
    std::vector<DWRITE_FONT_FEATURE> fontFeatures;
    if (!fontSettings.features.empty())
    {
        fontFeatures.reserve(fontSettings.features.size() + 3);

        // All of these features are enabled by default by DirectWrite.
        // If you want to (and can) peek into the source of DirectWrite
        // you can look for the "GenericDefaultGsubFeatures" and "GenericDefaultGposFeatures" arrays.
        // Gsub is for GetGlyphs() and Gpos for GetGlyphPlacements().
        //
        // GH#10774: Apparently specifying all of the features is just redundant.
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, 1);

        for (const auto& p : fontSettings.features)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                switch (const auto tag = DWRITE_MAKE_FONT_FEATURE_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES:
                    fontFeatures[0].parameter = p.second;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES:
                    fontFeatures[1].parameter = p.second;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES:
                    fontFeatures[2].parameter = p.second;
                    break;
                default:
                    fontFeatures.emplace_back(tag, p.second);
                    break;
                }
            }
        }
    }

    std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
    if (!fontSettings.axes.empty())
    {
        fontAxisValues.reserve(fontSettings.axes.size() + 3);

        // AtlasEngine::_recreateFontDependentResources() relies on these fields to
        // exist in this particular order in order to create appropriate default axes.
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_WEIGHT, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_ITALIC, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_SLANT, -1.0f);

        for (const auto& p : fontSettings.axes)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                switch (const auto tag = DWRITE_MAKE_FONT_AXIS_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_AXIS_TAG_WEIGHT:
                    fontAxisValues[0].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_ITALIC:
                    fontAxisValues[1].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_SLANT:
                    fontAxisValues[2].value = p.second;
                    break;
                default:
                    fontAxisValues.emplace_back(tag, p.second);
                    break;
                }
            }
        }
    }

    const auto font = _api.s.write()->font.write();
    _resolveFontMetrics(faceName, fontSettings, font);
    font->fontFeatures = std::move(fontFeatures);
    font->fontAxisValues = std::move(fontAxisValues);
}

void AtlasEngine::_resolveFontMetrics(const wchar_t* requestedFaceName, const FontSettings& fontSettings, ResolvedFontSettings* fontMetrics) const
{
    auto requestedWeight = fontSettings.weight;
    auto fontSize = fontSettings.fontSize;

    if (!requestedFaceName)
    {
        requestedFaceName = L"Consolas";
    }
    if (!fontSize)
    {
        fontSize = 12.0f;
    }
    if (!requestedWeight)
    {
        requestedWeight = DWRITE_FONT_WEIGHT_NORMAL;
    }

    // UpdateFont() (and its NearbyFontLoading feature path specifically) sets `_api.s->font->fontCollection`
    // to a custom font collection that includes .ttf files that are bundled with our app package. See GH#9375.
    // Doing it this way is a bit hacky, but it does have the benefit that we can cache a font collection
    // instance across font changes, like when zooming the font size rapidly using the scroll wheel.
    auto fontCollection = _api.s->font->fontCollection;
    if (!fontCollection)
    {
        THROW_IF_FAILED(_p.dwriteFactory->GetSystemFontCollection(fontCollection.addressof(), FALSE));
    }

    u32 index = 0;
    BOOL exists = false;
    THROW_IF_FAILED(fontCollection->FindFamilyName(requestedFaceName, &index, &exists));
    THROW_HR_IF(DWRITE_E_NOFONT, !exists);

    wil::com_ptr<IDWriteFontFamily> fontFamily;
    THROW_IF_FAILED(fontCollection->GetFontFamily(index, fontFamily.addressof()));

    wil::com_ptr<IDWriteFont> font;
    THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(requestedWeight), DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.addressof()));

    wil::com_ptr<IDWriteFontFace> fontFace;
    THROW_IF_FAILED(font->CreateFontFace(fontFace.addressof()));

    DWRITE_FONT_METRICS metrics{};
    fontFace->GetMetrics(&metrics);

    // Point sizes are commonly treated at a 72 DPI scale
    // (including by OpenType), whereas DirectWrite uses 96 DPI.
    // Since we want the height in px we multiply by the display's DPI.
    const auto dpi = static_cast<f32>(_api.s->font->dpi);
    const auto fontSizeInPx = fontSize / 72.0f * dpi;

    const auto designUnitsPerPx = fontSizeInPx / static_cast<f32>(metrics.designUnitsPerEm);
    const auto ascent = static_cast<f32>(metrics.ascent) * designUnitsPerPx;
    const auto descent = static_cast<f32>(metrics.descent) * designUnitsPerPx;
    const auto lineGap = static_cast<f32>(metrics.lineGap) * designUnitsPerPx;
    const auto underlinePosition = static_cast<f32>(-metrics.underlinePosition) * designUnitsPerPx;
    const auto underlineThickness = static_cast<f32>(metrics.underlineThickness) * designUnitsPerPx;
    const auto strikethroughPosition = static_cast<f32>(-metrics.strikethroughPosition) * designUnitsPerPx;
    const auto strikethroughThickness = static_cast<f32>(metrics.strikethroughThickness) * designUnitsPerPx;
    const auto advanceHeight = ascent + descent + lineGap;

    // We use the same character to determine the advance width as CSS for its "ch" unit ("0").
    // According to the CSS spec, if it's impossible to determine the advance width,
    // it must be assumed to be 0.5em wide. em in CSS refers to the computed font-size.
    auto advanceWidth = 0.5f * fontSizeInPx;
    {
        static constexpr u32 codePoint = '0';

        u16 glyphIndex;
        THROW_IF_FAILED(fontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex));

        if (glyphIndex)
        {
            DWRITE_GLYPH_METRICS glyphMetrics{};
            THROW_IF_FAILED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE));
            advanceWidth = static_cast<f32>(glyphMetrics.advanceWidth) * designUnitsPerPx;
        }
    }

    auto adjustedWidth = std::roundf(fontSettings.cellWidth.Resolve(advanceWidth, dpi, fontSizeInPx, advanceWidth));
    auto adjustedHeight = std::roundf(fontSettings.cellHeight.Resolve(advanceHeight, dpi, fontSizeInPx, advanceWidth));

    // Protection against bad user values in GetCellWidth/Y.
    // AtlasEngine fails hard with 0 cell sizes.
    adjustedWidth = std::max(1.0f, adjustedWidth);
    adjustedHeight = std::max(1.0f, adjustedHeight);

    const auto baseline = std::roundf(ascent + (lineGap + adjustedHeight - advanceHeight) / 2.0f);
    const auto underlinePos = std::roundf(baseline + underlinePosition);
    const auto underlineWidth = std::max(1.0f, std::roundf(underlineThickness));
    const auto strikethroughPos = std::roundf(baseline + strikethroughPosition);
    const auto strikethroughWidth = std::max(1.0f, std::roundf(strikethroughThickness));
    const auto thinLineWidth = std::max(1.0f, std::roundf(underlineThickness / 2.0f));

    // For double underlines we loosely follow what Word does:
    // 1. The lines are half the width of an underline (= thinLineWidth)
    // 2. Ideally the bottom line is aligned with the bottom of the underline
    // 3. The top underline is vertically in the middle between baseline and ideal bottom underline
    // 4. If the top line gets too close to the baseline the underlines are shifted downwards
    // 5. The minimum gap between the two lines appears to be similar to Tex (1.2pt)
    // (Additional notes below.)

    // 2.
    auto doubleUnderlinePosBottom = underlinePos + underlineWidth - thinLineWidth;
    // 3. Since we don't align the center of our two lines, but rather the top borders
    //    we need to subtract half a line width from our center point.
    auto doubleUnderlinePosTop = std::roundf((baseline + doubleUnderlinePosBottom - thinLineWidth) / 2.0f);
    // 4.
    doubleUnderlinePosTop = std::max(doubleUnderlinePosTop, baseline + thinLineWidth);
    // 5. The gap is only the distance _between_ the lines, but we need the distance from the
    //    top border of the top and bottom lines, which includes an additional line width.
    const auto doubleUnderlineGap = std::max(1.0f, std::roundf(1.2f / 72.0f * dpi));
    doubleUnderlinePosBottom = std::max(doubleUnderlinePosBottom, doubleUnderlinePosTop + doubleUnderlineGap + thinLineWidth);
    // Our cells can't overlap each other so we additionally clamp the bottom line to be inside the cell boundaries.
    doubleUnderlinePosBottom = std::min(doubleUnderlinePosBottom, adjustedHeight - thinLineWidth);

    const auto cellWidth = gsl::narrow<u16>(lrintf(adjustedWidth));
    const auto cellHeight = gsl::narrow<u16>(lrintf(adjustedHeight));

    if (fontMetrics)
    {
        std::wstring fontName{ requestedFaceName };
        const auto fontWeightU16 = gsl::narrow_cast<u16>(requestedWeight);
        const auto advanceWidthU16 = gsl::narrow_cast<u16>(lrintf(advanceWidth));
        const auto baselineU16 = gsl::narrow_cast<u16>(lrintf(baseline));
        const auto descenderU16 = gsl::narrow_cast<u16>(cellHeight - baselineU16);
        const auto thinLineWidthU16 = gsl::narrow_cast<u16>(lrintf(thinLineWidth));

        const auto gridBottomPositionU16 = gsl::narrow_cast<u16>(cellHeight - thinLineWidth);
        const auto gridRightPositionU16 = gsl::narrow_cast<u16>(cellWidth - thinLineWidth);

        const auto underlinePosU16 = gsl::narrow_cast<u16>(lrintf(underlinePos));
        const auto underlineWidthU16 = gsl::narrow_cast<u16>(lrintf(underlineWidth));
        const auto strikethroughPosU16 = gsl::narrow_cast<u16>(lrintf(strikethroughPos));
        const auto strikethroughWidthU16 = gsl::narrow_cast<u16>(lrintf(strikethroughWidth));
        const auto doubleUnderlinePosTopU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosTop));
        const auto doubleUnderlinePosBottomU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosBottom));

        // NOTE: From this point onward no early returns or throwing code should exist,
        // as we might cause _api to be in an inconsistent state otherwise.

        fontMetrics->fontCollection = std::move(fontCollection);
        fontMetrics->fontFamily = std::move(fontFamily);
        fontMetrics->fontName = std::move(fontName);
        fontMetrics->fontSize = fontSizeInPx;
        fontMetrics->cellSize = { cellWidth, cellHeight };
        fontMetrics->fontWeight = fontWeightU16;
        fontMetrics->advanceWidth = advanceWidthU16;
        fontMetrics->baseline = baselineU16;
        fontMetrics->descender = descenderU16;
        fontMetrics->thinLineWidth = thinLineWidthU16;

        fontMetrics->gridTop = { 0, thinLineWidthU16 };
        fontMetrics->gridBottom = { gridBottomPositionU16, thinLineWidthU16 };
        fontMetrics->gridLeft = { 0, thinLineWidthU16 };
        fontMetrics->gridRight = { gridRightPositionU16, thinLineWidthU16 };

        fontMetrics->underline = { underlinePosU16, underlineWidthU16 };
        fontMetrics->strikethrough = { strikethroughPosU16, strikethroughWidthU16 };
        fontMetrics->doubleUnderline[0] = { doubleUnderlinePosTopU16, thinLineWidthU16 };
        fontMetrics->doubleUnderline[1] = { doubleUnderlinePosBottomU16, thinLineWidthU16 };
        fontMetrics->overline = { 0, underlineWidthU16 };
    }
}
