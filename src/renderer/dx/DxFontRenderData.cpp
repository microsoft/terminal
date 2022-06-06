// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "DxFontRenderData.h"

#include <VersionHelpers.h>

#include "../base/FontCache.h"

static constexpr float POINTS_PER_INCH = 72.0f;
static constexpr std::wstring_view FALLBACK_LOCALE = L"en-us";
static constexpr size_t TAG_LENGTH = 4;

using namespace Microsoft::Console::Render;

DxFontRenderData::DxFontRenderData(::Microsoft::WRL::ComPtr<IDWriteFactory1> dwriteFactory) :
    _dwriteFactory(dwriteFactory),
    _nearbyCollection{ FontCache::GetCached() },
    _fontSize{},
    _glyphCell{},
    _lineMetrics{},
    _lineSpacing{}
{
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> DxFontRenderData::Analyzer()
{
    if (!_dwriteTextAnalyzer)
    {
        Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer;
        THROW_IF_FAILED(_dwriteFactory->CreateTextAnalyzer(&analyzer));
        THROW_IF_FAILED(analyzer.As(&_dwriteTextAnalyzer));
    }

    return _dwriteTextAnalyzer;
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFallback> DxFontRenderData::SystemFontFallback()
{
    if (!_systemFontFallback)
    {
        ::Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
        THROW_IF_FAILED(_dwriteFactory.As(&factory2));
        factory2->GetSystemFontFallback(&_systemFontFallback);
    }

    return _systemFontFallback;
}

[[nodiscard]] std::wstring DxFontRenderData::UserLocaleName()
{
    if (_userLocaleName.empty())
    {
        std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> localeName;

        const auto returnCode = GetUserDefaultLocaleName(localeName.data(), gsl::narrow<int>(localeName.size()));
        if (returnCode)
        {
            _userLocaleName = { localeName.data() };
        }
        else
        {
            _userLocaleName = { FALLBACK_LOCALE.data(), FALLBACK_LOCALE.size() };
        }
    }

    return _userLocaleName;
}

[[nodiscard]] til::size DxFontRenderData::GlyphCell() noexcept
{
    return _glyphCell;
}

[[nodiscard]] DxFontRenderData::LineMetrics DxFontRenderData::GetLineMetrics() noexcept
{
    return _lineMetrics;
}

[[nodiscard]] DWRITE_FONT_WEIGHT DxFontRenderData::DefaultFontWeight() noexcept
{
    return _defaultFontInfo.GetWeight();
}

[[nodiscard]] DWRITE_FONT_STYLE DxFontRenderData::DefaultFontStyle() noexcept
{
    return _defaultFontInfo.GetStyle();
}

[[nodiscard]] DWRITE_FONT_STRETCH DxFontRenderData::DefaultFontStretch() noexcept
{
    return _defaultFontInfo.GetStretch();
}

[[nodiscard]] const std::vector<DWRITE_FONT_FEATURE>& DxFontRenderData::DefaultFontFeatures() const noexcept
{
    return _featureVector;
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> DxFontRenderData::DefaultTextFormat()
{
    return TextFormatWithAttribute(_defaultFontInfo.GetWeight(), _defaultFontInfo.GetStyle(), _defaultFontInfo.GetStretch());
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::DefaultFontFace()
{
    return FontFaceWithAttribute(_defaultFontInfo.GetWeight(), _defaultFontInfo.GetStyle(), _defaultFontInfo.GetStretch());
}

[[nodiscard]] Microsoft::WRL::ComPtr<IBoxDrawingEffect> DxFontRenderData::DefaultBoxDrawingEffect()
{
    if (!_boxDrawingEffect)
    {
        // Calculate and cache the box effect for the base font. Scale is 1.0f because the base font is exactly the scale we want already.
        THROW_IF_FAILED(s_CalculateBoxEffect(DefaultTextFormat().Get(), _glyphCell.width, DefaultFontFace().Get(), 1.0f, &_boxDrawingEffect));
    }

    return _boxDrawingEffect;
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteTextFormat> DxFontRenderData::TextFormatWithAttribute(DWRITE_FONT_WEIGHT weight,
                                                                                                  DWRITE_FONT_STYLE style,
                                                                                                  DWRITE_FONT_STRETCH stretch)
{
    const auto textFormatIt = _textFormatMap.find(_ToMapKey(weight, style, stretch));
    if (textFormatIt == _textFormatMap.end())
    {
        auto fontInfo = _defaultFontInfo;
        fontInfo.SetWeight(weight);
        fontInfo.SetStyle(style);
        fontInfo.SetStretch(stretch);

        // Create the font with the fractional pixel height size.
        // It should have an integer pixel width by our math.
        // Then below, apply the line spacing to the format to position the floating point pixel height characters
        // into a cell that has an integer pixel height leaving some padding above/below as necessary to round them out.
        auto localeName = UserLocaleName();
        Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
        THROW_IF_FAILED(_BuildTextFormat(fontInfo, localeName).As(&textFormat));
        THROW_IF_FAILED(textFormat->SetLineSpacing(_lineSpacing.method, _lineSpacing.height, _lineSpacing.baseline));
        THROW_IF_FAILED(textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        THROW_IF_FAILED(textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

        _textFormatMap.emplace(_ToMapKey(weight, style, stretch), textFormat);
        return textFormat;
    }
    else
    {
        return textFormatIt->second;
    }
}

[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxFontRenderData::FontFaceWithAttribute(DWRITE_FONT_WEIGHT weight,
                                                                                               DWRITE_FONT_STYLE style,
                                                                                               DWRITE_FONT_STRETCH stretch)
{
    const auto fontFaceIt = _fontFaceMap.find(_ToMapKey(weight, style, stretch));
    if (fontFaceIt == _fontFaceMap.end())
    {
        auto fontInfo = _defaultFontInfo;
        fontInfo.SetWeight(weight);
        fontInfo.SetStyle(style);
        fontInfo.SetStretch(stretch);

        auto fontLocaleName = UserLocaleName();
        auto fontFace = fontInfo.ResolveFontFaceWithFallback(_nearbyCollection.get(), fontLocaleName);

        _fontFaceMap.emplace(_ToMapKey(weight, style, stretch), fontFace);
        return fontFace;
    }
    else
    {
        return fontFaceIt->second;
    }
}

// Routine Description:
// - Updates the font used for drawing
// Arguments:
// - desired - Information specifying the font that is requested
// - actual - Filled with the nearest font actually chosen for drawing
// - dpi - The DPI of the screen
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxFontRenderData::UpdateFont(const FontInfoDesired& desired, FontInfo& actual, const int dpi, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept
{
    try
    {
        _userLocaleName.clear();
        _textFormatMap.clear();
        _fontFaceMap.clear();
        _boxDrawingEffect.Reset();

        // Initialize the default font info and build everything from here.
        _defaultFontInfo = DxFontInfo(desired.GetFaceName(),
                                      desired.GetWeight(),
                                      DWRITE_FONT_STYLE_NORMAL,
                                      DWRITE_FONT_STRETCH_NORMAL);

        _SetFeatures(features);
        _SetAxes(axes);

        _BuildFontRenderData(desired, actual, dpi);
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Calculates the box drawing scale/translate matrix values to fit a box glyph into the cell as perfectly as possible.
// Arguments:
// - format - Text format used to determine line spacing (height including ascent & descent) as calculated from the base font.
// - widthPixels - The pixel width of the available cell.
// - face - The font face that is currently being used, may differ from the base font from the layout.
// - fontScale -  if the given font face is going to be scaled versus the format, we need to know so we can compensate for that. pass 1.0f for no scaling.
// - effect - Receives the effect to apply to box drawing characters. If no effect is received, special treatment isn't required.
// Return Value:
// - S_OK, GSL/WIL errors, DirectWrite errors, or math errors.
[[nodiscard]] HRESULT STDMETHODCALLTYPE DxFontRenderData::s_CalculateBoxEffect(IDWriteTextFormat* format, size_t widthPixels, IDWriteFontFace1* face, float fontScale, IBoxDrawingEffect** effect) noexcept
try
{
    // Check for bad in parameters.
    RETURN_HR_IF(E_INVALIDARG, !format);
    RETURN_HR_IF(E_INVALIDARG, !face);

    // Check the out parameter and fill it up with null.
    RETURN_HR_IF(E_INVALIDARG, !effect);
    *effect = nullptr;

    // The format is based around the main font that was specified by the user.
    // We need to know its size as well as the final spacing that was calculated around
    // it when it was first selected to get an idea of how large the bounding box is.
    const auto fontSize = format->GetFontSize();

    DWRITE_LINE_SPACING_METHOD spacingMethod;
    float lineSpacing; // total height of the cells
    float baseline; // vertical position counted down from the top where the characters "sit"
    RETURN_IF_FAILED(format->GetLineSpacing(&spacingMethod, &lineSpacing, &baseline));

    const auto ascentPixels = baseline;
    const auto descentPixels = lineSpacing - baseline;

    // We need this for the designUnitsPerEm which will be required to move back and forth between
    // Design Units and Pixels. I'll elaborate below.
    DWRITE_FONT_METRICS1 fontMetrics;
    face->GetMetrics(&fontMetrics);

    // If we had font fallback occur, the size of the font given to us (IDWriteFontFace1) can be different
    // than the font size used for the original format (IDWriteTextFormat).
    const auto scaledFontSize = fontScale * fontSize;

    // This is Unicode FULL BLOCK U+2588.
    // We presume that FULL BLOCK should be filling its entire cell in all directions so it should provide a good basis
    // in knowing exactly where to touch every single edge.
    // We're also presuming that the other box/line drawing glyphs were authored in this font to perfectly inscribe
    // inside of FULL BLOCK, with the same left/top/right/bottom bearings so they would look great when drawn adjacent.
    const UINT32 blockCodepoint = L'\x2588';

    // Get the index of the block out of the font.
    UINT16 glyphIndex;
    RETURN_IF_FAILED(face->GetGlyphIndicesW(&blockCodepoint, 1, &glyphIndex));

    // If it was 0, it wasn't found in the font. We're going to try again with
    // Unicode BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL U+253C which should be touching
    // all the edges of the possible rectangle, much like a full block should.
    if (glyphIndex == 0)
    {
        const UINT32 alternateCp = L'\x253C';
        RETURN_IF_FAILED(face->GetGlyphIndicesW(&alternateCp, 1, &glyphIndex));
    }

    // If we still didn't find the glyph index, we haven't implemented any further logic to figure out the box dimensions.
    // So we're just going to leave successfully as is and apply no scaling factor. It might look not-right, but it won't
    // stop the rendering pipeline.
    RETURN_HR_IF(S_FALSE, glyphIndex == 0);

    // Get the metrics of the given glyph, which we're going to treat as the outline box in which all line/block drawing
    // glyphs will be inscribed within, perfectly touching each edge as to align when two cells meet.
    DWRITE_GLYPH_METRICS boxMetrics = { 0 };
    RETURN_IF_FAILED(face->GetDesignGlyphMetrics(&glyphIndex, 1, &boxMetrics));

    // NOTE: All metrics we receive from DWRITE are going to be in "design units" which are a somewhat agnostic
    //       way of describing proportions.
    //       Converting back and forth between real pixels and design units is possible using
    //       any font's specific fontSize and the designUnitsPerEm FONT_METRIC value.
    //
    // Here's what to know about the boxMetrics:
    //
    //
    //
    //   topLeft --> +--------------------------------+    ---
    //               |         ^                      |     |
    //               |         |  topSide             |     |
    //               |         |  Bearing             |     |
    //               |         v                      |     |
    //               |      +-----------------+       |     |
    //               |      |                 |       |     |
    //               |      |                 |       |     | a
    //               |      |                 |       |     | d
    //               |      |                 |       |     | v
    //               +<---->+                 |       |     | a
    //               |      |                 |       |     | n
    //               | left |                 |       |     | c
    //               | Side |                 |       |     | e
    //               | Bea- |                 |       |     | H
    //               | ring |                 | right |     | e
    //  vertical     |      |                 | Side  |     | i
    //  OriginY -->  x      |                 | Bea-  |     | g
    //               |      |                 | ring  |     | h
    //               |      |                 |       |     | t
    //               |      |                 +<----->+     |
    //               |      +-----------------+       |     |
    //               |                     ^          |     |
    //               |       bottomSide    |          |     |
    //               |          Bearing    |          |     |
    //               |                     v          |     |
    //               +--------------------------------+    ---
    //
    //
    //               |                                |
    //               +--------------------------------+
    //               |         advanceWidth           |
    //
    //
    // NOTE: The bearings can be negative, in which case it is specifying that the glyphs overhang the box
    // as defined by the advanceHeight/width.
    // See also: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_glyph_metrics

    // The scale is a multiplier and the translation is addition. So *1 and +0 will mean nothing happens.
    const auto defaultBoxVerticalScaleFactor = 1.0f;
    auto boxVerticalScaleFactor = defaultBoxVerticalScaleFactor;
    const auto defaultBoxVerticalTranslation = 0.0f;
    auto boxVerticalTranslation = defaultBoxVerticalTranslation;
    {
        // First, find the dimensions of the glyph representing our fully filled box.

        // Ascent is how far up from the baseline we'll draw.
        // verticalOriginY is the measure from the topLeft corner of the bounding box down to where
        // the glyph's version of the baseline is.
        // topSideBearing is how much "gap space" is left between that topLeft and where the glyph
        // starts drawing. Subtract the gap space to find how far is drawn upward from baseline.
        const auto boxAscentDesignUnits = boxMetrics.verticalOriginY - boxMetrics.topSideBearing;

        // Descent is how far down from the baseline we'll draw.
        // advanceHeight is the total height of the drawn bounding box.
        // verticalOriginY is how much was given to the ascent, so subtract that out.
        // What remains is then the descent value. Remove the
        // bottomSideBearing as the "gap space" on the bottom to find how far is drawn downward from baseline.
        const auto boxDescentDesignUnits = boxMetrics.advanceHeight - boxMetrics.verticalOriginY - boxMetrics.bottomSideBearing;

        // The height, then, of the entire box is just the sum of the ascent above the baseline and the descent below.
        const auto boxHeightDesignUnits = boxAscentDesignUnits + boxDescentDesignUnits;

        // Second, find the dimensions of the cell we're going to attempt to fit within.
        // We know about the exact ascent/descent units in pixels as calculated when we chose a font and
        // adjusted the ascent/descent for a nice perfect baseline and integer total height.
        // All we need to do is adapt it into Design Units so it meshes nicely with the Design Units above.
        // Use the formula: Pixels * Design Units Per Em / Font Size = Design Units
        const auto cellAscentDesignUnits = ascentPixels * fontMetrics.designUnitsPerEm / scaledFontSize;
        const auto cellDescentDesignUnits = descentPixels * fontMetrics.designUnitsPerEm / scaledFontSize;
        const auto cellHeightDesignUnits = cellAscentDesignUnits + cellDescentDesignUnits;

        // OK, now do a few checks. If the drawn box touches the top and bottom of the cell
        // and the box is overall tall enough, then we'll not bother adjusting.
        // We will presume the font author has set things as they wish them to be.
        const auto boxTouchesCellTop = boxAscentDesignUnits >= cellAscentDesignUnits;
        const auto boxTouchesCellBottom = boxDescentDesignUnits >= cellDescentDesignUnits;
        const auto boxIsTallEnoughForCell = boxHeightDesignUnits >= cellHeightDesignUnits;

        // If not...
        if (!(boxTouchesCellTop && boxTouchesCellBottom && boxIsTallEnoughForCell))
        {
            // Find a scaling factor that will make the total height drawn of this box
            // perfectly fit the same number of design units as the cell.
            // Since scale factor is a multiplier, it doesn't matter that this is design units.
            // The fraction between the two heights in pixels should be exactly the same
            // (which is what will matter when we go to actually render it... the pixels that is.)
            // Don't scale below 1.0. If it'd shrink, just center it at the prescribed scale.
            boxVerticalScaleFactor = std::max(cellHeightDesignUnits / boxHeightDesignUnits, 1.0f);

            // The box as scaled might be hanging over the top or bottom of the cell (or both).
            // We find out the amount of overhang/underhang on both the top and the bottom.
            const auto extraAscent = boxAscentDesignUnits * boxVerticalScaleFactor - cellAscentDesignUnits;
            const auto extraDescent = boxDescentDesignUnits * boxVerticalScaleFactor - cellDescentDesignUnits;

            // This took a bit of time and effort and it's difficult to put into words, but here goes.
            // We want the average of the two magnitudes to find out how much to "take" from one and "give"
            // to the other such that both are equal. We presume the glyphs are designed to be drawn
            // centered in their box vertically to look good.
            // The ordering around subtraction is required to ensure that the direction is correct with a negative
            // translation moving up (taking excess descent and adding to ascent) and positive is the opposite.
            const auto boxVerticalTranslationDesignUnits = (extraAscent - extraDescent) / 2;

            // The translation is just a raw movement of pixels up or down. Since we were working in Design Units,
            // we need to run the opposite algorithm shown above to go from Design Units to Pixels.
            boxVerticalTranslation = boxVerticalTranslationDesignUnits * scaledFontSize / fontMetrics.designUnitsPerEm;
        }
    }

    // The horizontal adjustments follow the exact same logic as the vertical ones.
    const auto defaultBoxHorizontalScaleFactor = 1.0f;
    auto boxHorizontalScaleFactor = defaultBoxHorizontalScaleFactor;
    const auto defaultBoxHorizontalTranslation = 0.0f;
    auto boxHorizontalTranslation = defaultBoxHorizontalTranslation;
    {
        // This is the only difference. We don't have a horizontalOriginX from the metrics.
        // However, https://docs.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_glyph_metrics says
        // the X coordinate is specified by half the advanceWidth to the right of the horizontalOrigin.
        // So we'll use that as the "center" and apply it the role that verticalOriginY had above.

        const auto boxCenterDesignUnits = boxMetrics.advanceWidth / 2;
        const auto boxLeftDesignUnits = boxCenterDesignUnits - boxMetrics.leftSideBearing;
        const auto boxRightDesignUnits = boxMetrics.advanceWidth - boxMetrics.rightSideBearing - boxCenterDesignUnits;
        const auto boxWidthDesignUnits = boxLeftDesignUnits + boxRightDesignUnits;

        const auto cellWidthDesignUnits = widthPixels * fontMetrics.designUnitsPerEm / scaledFontSize;
        const auto cellLeftDesignUnits = cellWidthDesignUnits / 2;
        const auto cellRightDesignUnits = cellLeftDesignUnits;

        const auto boxTouchesCellLeft = boxLeftDesignUnits >= cellLeftDesignUnits;
        const auto boxTouchesCellRight = boxRightDesignUnits >= cellRightDesignUnits;
        const auto boxIsWideEnoughForCell = boxWidthDesignUnits >= cellWidthDesignUnits;

        if (!(boxTouchesCellLeft && boxTouchesCellRight && boxIsWideEnoughForCell))
        {
            boxHorizontalScaleFactor = std::max(cellWidthDesignUnits / boxWidthDesignUnits, 1.0f);
            const auto extraLeft = boxLeftDesignUnits * boxHorizontalScaleFactor - cellLeftDesignUnits;
            const auto extraRight = boxRightDesignUnits * boxHorizontalScaleFactor - cellRightDesignUnits;

            const auto boxHorizontalTranslationDesignUnits = (extraLeft - extraRight) / 2;

            boxHorizontalTranslation = boxHorizontalTranslationDesignUnits * scaledFontSize / fontMetrics.designUnitsPerEm;
        }
    }

    // If we set anything, make a drawing effect. Otherwise, there isn't one.
    if (defaultBoxVerticalScaleFactor != boxVerticalScaleFactor ||
        defaultBoxVerticalTranslation != boxVerticalTranslation ||
        defaultBoxHorizontalScaleFactor != boxHorizontalScaleFactor ||
        defaultBoxHorizontalTranslation != boxHorizontalTranslation)
    {
        // OK, make the object that will represent our effect, stuff the metrics into it, and return it.
        RETURN_IF_FAILED(WRL::MakeAndInitialize<BoxDrawingEffect>(effect, boxVerticalScaleFactor, boxVerticalTranslation, boxHorizontalScaleFactor, boxHorizontalTranslation));
    }

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Returns whether the user set or updated any of the font features to be applied
bool DxFontRenderData::DidUserSetFeatures() const noexcept
{
    return _didUserSetFeatures;
}

// Routine Description:
// - Returns whether the user set or updated any of the font axes to be applied
bool DxFontRenderData::DidUserSetAxes() const noexcept
{
    return _didUserSetAxes;
}

// Routine Description:
// - Function called to inform us whether to use the user set weight
//   in the font axes
// - Called by CustomTextLayout, when the text attribute is intense we should
//   ignore the user set weight, otherwise setting the bold font axis
//   breaks the bold font attribute
// Arguments:
// - inhibitUserWeight: boolean that tells us if we should use the user set weight
//   in the font axes
void DxFontRenderData::InhibitUserWeight(bool inhibitUserWeight) noexcept
{
    _inhibitUserWeight = inhibitUserWeight;
}

// Routine Description:
// - Returns whether the set italic in the font axes
// Return Value:
// - True if the user set the italic axis to 1,
//   false if the italic axis is not present or the italic axis is set to 0
bool DxFontRenderData::DidUserSetItalic() const noexcept
{
    return _didUserSetItalic;
}

// Routine Description:
// - Updates our internal map of font features with the given features
// - NOTE TO CALLER: Make sure to call _BuildFontRenderData after calling this for the feature changes
//   to take place
// Arguments:
// - features - the features to update our map with
void DxFontRenderData::_SetFeatures(const std::unordered_map<std::wstring_view, uint32_t>& features)
{
    // Populate the feature map with the standard list first
    std::unordered_map<DWRITE_FONT_FEATURE_TAG, uint32_t> featureMap{
        { DWRITE_MAKE_FONT_FEATURE_TAG('c', 'a', 'l', 't'), 1 }, // Contextual Alternates
        { DWRITE_MAKE_FONT_FEATURE_TAG('l', 'i', 'g', 'a'), 1 }, // Standard Ligatures
        { DWRITE_MAKE_FONT_FEATURE_TAG('c', 'l', 'i', 'g'), 1 }, // Contextual Ligatures
        { DWRITE_MAKE_FONT_FEATURE_TAG('k', 'e', 'r', 'n'), 1 } // Kerning
    };

    // Update our feature map with the provided features
    if (!features.empty())
    {
        for (const auto [tag, param] : features)
        {
            if (tag.length() == TAG_LENGTH)
            {
                featureMap.insert_or_assign(DWRITE_MAKE_FONT_FEATURE_TAG(til::at(tag, 0), til::at(tag, 1), til::at(tag, 2), til::at(tag, 3)), param);
            }
        }
        _didUserSetFeatures = true;
    }
    else
    {
        _didUserSetFeatures = false;
    }

    // Convert the data to DWRITE_FONT_FEATURE and store it in a vector for CustomTextLayout
    _featureVector.clear();
    for (const auto [tag, param] : featureMap)
    {
        _featureVector.push_back(DWRITE_FONT_FEATURE{ tag, param });
    }
}

// Routine Description:
// - Updates our internal map of font axes with the given axes
// - NOTE TO CALLER: Make sure to call _BuildFontRenderData after calling this for the axes changes
//   to take place
// Arguments:
// - axes - the axes to update our map with
void DxFontRenderData::_SetAxes(const std::unordered_map<std::wstring_view, float>& axes)
{
    // Clear out the old vector and booleans in case this is a hot reload
    _axesVector = std::vector<DWRITE_FONT_AXIS_VALUE>{};
    _didUserSetAxes = false;
    _didUserSetItalic = false;

    // Update our axis map with the provided axes
    if (!axes.empty())
    {
        // Store the weight aside: we will be creating a span of all the axes in the vector except the weight,
        // and then we will add the weight to the vector
        // We are doing this so that when the text attribute is intense, we can apply all the axes except the weight
        std::optional<DWRITE_FONT_AXIS_VALUE> weightAxis;

        // Since we are calling an 'emplace_back' after creating the span,
        // there is a chance a reallocation happens (if the vector needs to grow), which would make the span point to
        // deallocated memory. To avoid this, make sure to reserve enough memory in the vector.
        _axesVector.reserve(axes.size());

#pragma warning(suppress : 26445) // the analyzer doesn't like reference to string_view
        for (const auto& [axis, value] : axes)
        {
            if (axis.length() == TAG_LENGTH)
            {
                const auto dwriteFontAxis = DWRITE_FONT_AXIS_VALUE{ DWRITE_MAKE_FONT_AXIS_TAG(til::at(axis, 0), til::at(axis, 1), til::at(axis, 2), til::at(axis, 3)), value };
                if (dwriteFontAxis.axisTag != DWRITE_FONT_AXIS_TAG_WEIGHT)
                {
                    _axesVector.emplace_back(dwriteFontAxis);
                }
                else
                {
                    weightAxis = dwriteFontAxis;
                }
                _didUserSetItalic |= dwriteFontAxis.axisTag == DWRITE_FONT_AXIS_TAG_ITALIC && value == 1;
            }
        }

        // Make the span, which has all the axes except the weight
        _axesVectorWithoutWeight = gsl::make_span(_axesVector);

        // Add the weight axis to the vector if needed
        if (weightAxis)
        {
            _axesVector.emplace_back(weightAxis.value());
        }
        _didUserSetAxes = true;
    }
}

// Method Description:
// - Converts a DWRITE_FONT_STRETCH enum into the corresponding float value to
//   create a DWRITE_FONT_AXIS_VALUE with
// Arguments:
// - fontStretch: the old DWRITE_FONT_STRETCH enum to be converted into an axis value
// Return value:
// - The float value corresponding to the passed in fontStretch
float DxFontRenderData::_FontStretchToWidthAxisValue(DWRITE_FONT_STRETCH fontStretch) noexcept
{
    // 10 elements from DWRITE_FONT_STRETCH_UNDEFINED (0) to DWRITE_FONT_STRETCH_ULTRA_EXPANDED (9)
    static constexpr auto fontStretchEnumToVal = std::array{ 100.0f, 50.0f, 62.5f, 75.0f, 87.5f, 100.0f, 112.5f, 125.0f, 150.0f, 200.0f };

    if (gsl::narrow_cast<size_t>(fontStretch) > fontStretchEnumToVal.size())
    {
        fontStretch = DWRITE_FONT_STRETCH_NORMAL;
    }

    return til::at(fontStretchEnumToVal, fontStretch);
}

// Method Description:
// - Converts a DWRITE_FONT_STYLE enum into the corresponding float value to
//   create a DWRITE_FONT_AXIS_VALUE with
// Arguments:
// - fontStyle: the old DWRITE_FONT_STYLE enum to be converted into an axis value
// Return value:
// - The float value corresponding to the passed in fontStyle
float DxFontRenderData::_FontStyleToSlantFixedAxisValue(DWRITE_FONT_STYLE fontStyle) noexcept
{
    // DWRITE_FONT_STYLE_NORMAL (0), DWRITE_FONT_STYLE_OBLIQUE (1), DWRITE_FONT_STYLE_ITALIC (2)
    static constexpr auto fontStyleEnumToVal = std::array{ 0.0f, -20.0f, -12.0f };

    // Both DWRITE_FONT_STYLE_OBLIQUE and DWRITE_FONT_STYLE_ITALIC default to having slant.
    // Though an italic font technically need not have slant (there exist upright ones), the
    // vast majority of italic fonts are also slanted. Ideally the slant comes from the
    // 'slnt' value in the STAT or fvar table, or the post table italic angle.

    if (gsl::narrow_cast<size_t>(fontStyle) > fontStyleEnumToVal.size())
    {
        fontStyle = DWRITE_FONT_STYLE_NORMAL;
    }

    return til::at(fontStyleEnumToVal, fontStyle);
}

// Method Description:
// - Fill any missing axis values that might be known but were unspecified, such as omitting
//   the 'wght' axis tag but specifying the old DWRITE_FONT_WEIGHT enum
// - This function will only be called with a valid IDWriteTextFormat3
//   (on platforms where IDWriteTextFormat3 is supported)
// Arguments:
// - fontWeight: the old DWRITE_FONT_WEIGHT enum to be converted into an axis value
// - fontStretch: the old DWRITE_FONT_STRETCH enum to be converted into an axis value
// - fontStyle: the old DWRITE_FONT_STYLE enum to be converted into an axis value
// - fontSize: the number to convert into an axis value
// - format: the IDWriteTextFormat3 to get the defined axes from
// Return value:
// - The fully formed axes vector
#pragma warning(suppress : 26429) // the analyzer doesn't detect that our FAIL_FAST_IF_NULL macro \
    // checks format for nullness
std::vector<DWRITE_FONT_AXIS_VALUE> DxFontRenderData::GetAxisVector(const DWRITE_FONT_WEIGHT fontWeight,
                                                                    const DWRITE_FONT_STRETCH fontStretch,
                                                                    const DWRITE_FONT_STYLE fontStyle,
                                                                    IDWriteTextFormat3* format)
{
    FAIL_FAST_IF_NULL(format);

    const auto axesCount = format->GetFontAxisValueCount();
    std::vector<DWRITE_FONT_AXIS_VALUE> axesVector;
    axesVector.resize(axesCount);
    format->GetFontAxisValues(axesVector.data(), axesCount);

    auto axisTagPresence = AxisTagPresence::None;
    for (const auto& fontAxisValue : axesVector)
    {
        switch (fontAxisValue.axisTag)
        {
        case DWRITE_FONT_AXIS_TAG_WEIGHT:
            WI_SetFlag(axisTagPresence, AxisTagPresence::Weight);
            break;
        case DWRITE_FONT_AXIS_TAG_WIDTH:
            WI_SetFlag(axisTagPresence, AxisTagPresence::Width);
            break;
        case DWRITE_FONT_AXIS_TAG_ITALIC:
            WI_SetFlag(axisTagPresence, AxisTagPresence::Italic);
            break;
        case DWRITE_FONT_AXIS_TAG_SLANT:
            WI_SetFlag(axisTagPresence, AxisTagPresence::Slant);
            break;
        }
    }

    if (WI_IsFlagClear(axisTagPresence, AxisTagPresence::Weight))
    {
        axesVector.emplace_back(DWRITE_FONT_AXIS_VALUE{ DWRITE_FONT_AXIS_TAG_WEIGHT, gsl::narrow<float>(fontWeight) });
    }
    if (WI_IsFlagClear(axisTagPresence, AxisTagPresence::Width))
    {
        axesVector.emplace_back(DWRITE_FONT_AXIS_VALUE{ DWRITE_FONT_AXIS_TAG_WIDTH, _FontStretchToWidthAxisValue(fontStretch) });
    }
    if (WI_IsFlagClear(axisTagPresence, AxisTagPresence::Italic))
    {
        axesVector.emplace_back(DWRITE_FONT_AXIS_VALUE{ DWRITE_FONT_AXIS_TAG_ITALIC, (fontStyle == DWRITE_FONT_STYLE_ITALIC ? 1.0f : 0.0f) });
    }
    if (WI_IsFlagClear(axisTagPresence, AxisTagPresence::Slant))
    {
        axesVector.emplace_back(DWRITE_FONT_AXIS_VALUE{ DWRITE_FONT_AXIS_TAG_SLANT, _FontStyleToSlantFixedAxisValue(fontStyle) });
    }

    return axesVector;
}

// Routine Description:
// - Build the needed data for rendering according to the font used
// Arguments:
// - desired - Information specifying the font that is requested
// - actual - Filled with the nearest font actually chosen for drawing
// - dpi - The DPI of the screen
// Return Value:
// - None
void DxFontRenderData::_BuildFontRenderData(const FontInfoDesired& desired, FontInfo& actual, const int dpi)
{
    auto fontLocaleName = UserLocaleName();
    // This is the first attempt to resolve font face after `UpdateFont`.
    // Note that the following line may cause property changes _inside_ `_defaultFontInfo` because the desired font may not exist.
    // See the implementation of `ResolveFontFaceWithFallback` for details.
    const auto face = _defaultFontInfo.ResolveFontFaceWithFallback(_nearbyCollection.get(), fontLocaleName);

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
    auto heightDesired = desired.GetEngineSize().Y * USER_DEFAULT_SCREEN_DPI / POINTS_PER_INCH;

    // The advance is the number of pixels left-to-right (X dimension) for the given font.
    // We're finding a proportional factor here with the design units in "ems", not an actual pixel measurement.

    // Now we play trickery with the font size. Scale by the DPI to get the height we expect.
    heightDesired *= (static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI));

    const auto widthAdvance = static_cast<float>(advanceInDesignUnits) / fontMetrics.designUnitsPerEm;

    // Use the real pixel height desired by the "em" factor for the width to get the number of pixels
    // we will need per character in width. This will almost certainly result in fractional X-dimension pixels.
    const auto widthApprox = heightDesired * widthAdvance;

    // Since we can't deal with columns of the presentation grid being fractional pixels in width, round to the nearest whole pixel.
    const auto widthExact = round(widthApprox);

    // Now reverse the "em" factor from above to turn the exact pixel width into a (probably) fractional
    // height in pixels of each character. It's easier for us to pad out height and align vertically
    // than it is horizontally.
    const auto fontSize = widthExact / widthAdvance;
    _fontSize = fontSize;

    // Now figure out the basic properties of the character height which include ascent and descent
    // for this specific font size.
    const auto ascent = (fontSize * fontMetrics.ascent) / fontMetrics.designUnitsPerEm;
    const auto descent = (fontSize * fontMetrics.descent) / fontMetrics.designUnitsPerEm;

    // Get the gap.
    const auto gap = (fontSize * fontMetrics.lineGap) / fontMetrics.designUnitsPerEm;
    const auto halfGap = gap / 2;

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

    _lineSpacing = lineSpacing;

    // The scaled size needs to represent the pixel box that each character will fit within for the purposes
    // of hit testing math and other such multiplication/division.
    til::size coordSize;
    coordSize.X = static_cast<til::CoordType>(widthExact);
    coordSize.Y = static_cast<til::CoordType>(lineSpacing.height);

    // Unscaled is for the purposes of re-communicating this font back to the renderer again later.
    // As such, we need to give the same original size parameter back here without padding
    // or rounding or scaling manipulation.
    const auto unscaled = desired.GetEngineSize();

    const auto scaled = coordSize;

    actual.SetFromEngine(_defaultFontInfo.GetFamilyName(),
                         desired.GetFamily(),
                         DefaultTextFormat()->GetFontWeight(),
                         false,
                         scaled,
                         unscaled);

    actual.SetFallback(_defaultFontInfo.GetFallback());

    LineMetrics lineMetrics;
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
    const auto maxUnderlineOffset = lineSpacing.height - lineMetrics.underlineWidth;
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

    _lineMetrics = lineMetrics;

    _glyphCell = actual.GetSize();
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> DxFontRenderData::_BuildTextFormat(const DxFontInfo& fontInfo, const std::wstring_view localeName)
{
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    THROW_IF_FAILED(_dwriteFactory->CreateTextFormat(fontInfo.GetFamilyName().data(),
                                                     _nearbyCollection.get(),
                                                     fontInfo.GetWeight(),
                                                     fontInfo.GetStyle(),
                                                     fontInfo.GetStretch(),
                                                     _fontSize,
                                                     localeName.data(),
                                                     &format));

    // If the OS supports IDWriteTextFormat3, set the font axes
    ::Microsoft::WRL::ComPtr<IDWriteTextFormat3> format3;
    if (!FAILED(format->QueryInterface(IID_PPV_ARGS(&format3))))
    {
        if (_inhibitUserWeight && !_axesVectorWithoutWeight.empty())
        {
            format3->SetFontAxisValues(_axesVectorWithoutWeight.data(), gsl::narrow<uint32_t>(_axesVectorWithoutWeight.size()));
        }
        else if (!_inhibitUserWeight && !_axesVector.empty())
        {
            format3->SetFontAxisValues(_axesVector.data(), gsl::narrow<uint32_t>(_axesVector.size()));
        }
    }

    return format;
}
