// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CustomTextLayout.h"
#include "CustomTextRenderer.h"

#include <wrl.h>
#include <wrl/client.h>
#include <VersionHelpers.h>

#include "BoxDrawingEffect.h"

using namespace Microsoft::Console::Render;

// Routine Description:
// - Creates a CustomTextLayout object for calculating which glyphs should be placed and where
// Arguments:
// - dxFontRenderData - The DirectWrite font render data for our layout
CustomTextLayout::CustomTextLayout(const gsl::not_null<DxFontRenderData*> fontRenderData) :
    _fontRenderData{ fontRenderData },
    _formatInUse{ fontRenderData->DefaultTextFormat().Get() },
    _fontInUse{ fontRenderData->DefaultFontFace().Get() },
    _numberSubstitution{},
    _readingDirection{ DWRITE_READING_DIRECTION_LEFT_TO_RIGHT },
    _runs{},
    _breakpoints{},
    _runIndex{ 0 },
    _width{ gsl::narrow_cast<size_t>(fontRenderData->GlyphCell().width) },
    _isEntireTextSimple{ false }
{
    _localeName.resize(gsl::narrow_cast<size_t>(fontRenderData->DefaultTextFormat()->GetLocaleNameLength()) + 1); // +1 for null
    THROW_IF_FAILED(fontRenderData->DefaultTextFormat()->GetLocaleName(_localeName.data(), gsl::narrow<UINT32>(_localeName.size())));
}

//Routine Description:
// - Resets this custom text layout to the freshly allocated state in terms of text analysis.
// Arguments:
// - <none>, modifies internal state
// Return Value:
// - S_OK or suitable memory management issue
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::Reset() noexcept
try
{
    _runs.clear();
    _breakpoints.clear();
    _runIndex = 0;
    _isEntireTextSimple = false;
    _textClusterColumns.clear();
    _text.clear();
    _glyphScaleCorrections.clear();
    _glyphClusters.clear();
    _glyphIndices.clear();
    _glyphDesignUnitAdvances.clear();
    _glyphAdvances.clear();
    _glyphOffsets.clear();
    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Appends text to this layout for analysis/processing.
// Arguments:
// - clusters - From the backing buffer, the text to be displayed clustered by the columns it should consume.
// Return Value:
// - S_OK or suitable memory management issue.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::AppendClusters(const std::span<const ::Microsoft::Console::Render::Cluster> clusters)
try
{
    _textClusterColumns.reserve(_textClusterColumns.size() + clusters.size());

    for (const auto& cluster : clusters)
    {
        const auto cols = gsl::narrow<UINT16>(cluster.GetColumns());
        const auto text = cluster.GetText();

        // Push back the number of columns for this bit of text.
        _textClusterColumns.push_back(cols);

        // If there is more than one text character here, push 0s for the rest of the columns
        // of the text run.
        _textClusterColumns.resize(_textClusterColumns.size() + base::ClampSub(text.size(), 1u), gsl::narrow_cast<UINT16>(0u));

        _text += text;
    }

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Figures out how many columns this layout should take. This will use the analyze step only.
// Arguments:
// - columns - The number of columns the layout should consume when done.
// Return Value:
// - S_OK or suitable DirectX/DirectWrite/Direct2D result code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::GetColumns(_Out_ UINT32* columns)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, columns);
    *columns = 0;

    _formatInUse = _fontRenderData->DefaultTextFormat().Get();
    _fontInUse = _fontRenderData->DefaultFontFace().Get();

    RETURN_IF_FAILED(_AnalyzeTextComplexity());
    RETURN_IF_FAILED(_AnalyzeRuns());
    RETURN_IF_FAILED(_ShapeGlyphRuns());

    const auto totalAdvance = std::accumulate(_glyphAdvances.cbegin(), _glyphAdvances.cend(), 0.0f);

    *columns = static_cast<UINT32>(ceil(totalAdvance / _width));

    return S_OK;
}

// Routine Description:
// - Implements a drawing interface similarly to the default IDWriteTextLayout which will
//   take the string from construction, analyze it for complexity, shape up the glyphs,
//   and then draw the final product to the given renderer at the point and pass along
//   the context information.
// - This specific class does the layout calculations and complexity analysis, not the
//   final drawing. That's the renderer's job (passed in.)
// Arguments:
// - clientDrawingContext - Optional pointer to information that the renderer might need
//                          while attempting to graphically place the text onto the screen
// - renderer - The interface to be used for actually putting text onto the screen
// - originX - X pixel point of top left corner on final surface for drawing
// - originY - Y pixel point of top left corner on final surface for drawing
// Return Value:
// - S_OK or suitable DirectX/DirectWrite/Direct2D result code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::Draw(_In_opt_ void* clientDrawingContext,
                                                               _In_ IDWriteTextRenderer* renderer,
                                                               FLOAT originX,
                                                               FLOAT originY) noexcept
try
{
    const auto drawingContext = static_cast<const DrawingContext*>(clientDrawingContext);

    auto weight = _fontRenderData->DefaultFontWeight();
    auto style = _fontRenderData->DefaultFontStyle();
    const auto stretch = _fontRenderData->DefaultFontStretch();

    if (drawingContext->useBoldFont)
    {
        // TODO: "relative" bold?
        weight = DWRITE_FONT_WEIGHT_BOLD;
        // Since we are setting the font weight according to the text attribute,
        // make sure to tell the text format to ignore the user set font weight
        _fontRenderData->InhibitUserWeight(true);
    }
    else
    {
        _fontRenderData->InhibitUserWeight(false);
    }

    if (drawingContext->useItalicFont || _fontRenderData->DidUserSetItalic())
    {
        style = DWRITE_FONT_STYLE_ITALIC;
    }

    _formatInUse = _fontRenderData->TextFormatWithAttribute(weight, style, stretch).Get();
    _fontInUse = _fontRenderData->FontFaceWithAttribute(weight, style, stretch).Get();

    RETURN_IF_FAILED(_AnalyzeTextComplexity());
    RETURN_IF_FAILED(_AnalyzeRuns());
    RETURN_IF_FAILED(_ShapeGlyphRuns());
    RETURN_IF_FAILED(_CorrectGlyphRuns());
    // Correcting box drawing has to come after both font fallback and
    // the glyph run advance correction (which will apply a font size scaling factor).
    // We need to know all the proposed X and Y dimension metrics to get this right.
    RETURN_IF_FAILED(_CorrectBoxDrawing());

    RETURN_IF_FAILED(_DrawGlyphRuns(clientDrawingContext, renderer, { originX, originY }));

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Uses the internal text information and the analyzers/font information from construction
//   to determine the complexity of the text. If the text is determined to be entirely simple,
//   we'll have more chances to optimize the layout process.
// Arguments:
// - <none> - Uses internal state
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_AnalyzeTextComplexity() noexcept
{
    try
    {
        const auto textLength = gsl::narrow<UINT32>(_text.size());

        auto isTextSimple = FALSE;
        UINT32 uiLengthRead = 0;

        // Start from the beginning.
        const UINT32 glyphStart = 0;

        _glyphIndices.resize(textLength);

        const auto hr = _fontRenderData->Analyzer()->GetTextComplexity(
            _text.c_str(),
            textLength,
            _fontInUse,
            &isTextSimple,
            &uiLengthRead,
            &_glyphIndices.at(glyphStart));

        RETURN_IF_FAILED(hr);

        _isEntireTextSimple = isTextSimple && uiLengthRead == textLength;
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Uses the internal text information and the analyzers/font information from construction
//   to determine the complexity of the text inside this layout, compute the subsections (or runs)
//   that contain similar property information, and stores that information internally.
// - We determine line breakpoints, bidirectional information, the script properties,
//   number substitution, and font fallback properties in this function.
// Arguments:
// - <none> - Uses internal state
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_AnalyzeRuns() noexcept
{
    try
    {
        // We're going to need the text length in UINT32 format for the DWrite calls.
        // Convert it once up front.
        const auto textLength = gsl::narrow<UINT32>(_text.size());

        // Initially start out with one result that covers the entire range.
        // This result will be subdivided by the analysis processes.
        _runs.resize(1);
        auto& initialRun = _runs.front();
        initialRun.textLength = textLength;
        initialRun.bidiLevel = (_readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);

        // Allocate enough room to have one breakpoint per code unit.
        _breakpoints.resize(_text.size());

        if (!_isEntireTextSimple || _fontRenderData->DidUserSetAxes())
        {
            // Call each of the analyzers in sequence, recording their results.
            RETURN_IF_FAILED(_fontRenderData->Analyzer()->AnalyzeLineBreakpoints(this, 0, textLength, this));
            RETURN_IF_FAILED(_fontRenderData->Analyzer()->AnalyzeBidi(this, 0, textLength, this));
            RETURN_IF_FAILED(_fontRenderData->Analyzer()->AnalyzeScript(this, 0, textLength, this));
            RETURN_IF_FAILED(_fontRenderData->Analyzer()->AnalyzeNumberSubstitution(this, 0, textLength, this));
            // Perform our custom font fallback analyzer that mimics the pattern of the real analyzers.
            RETURN_IF_FAILED(_AnalyzeFontFallback(this, 0, textLength));
        }

        // Ensure that a font face is attached to every run
        for (auto& run : _runs)
        {
            if (!run.fontFace)
            {
                run.fontFace = _fontInUse;
            }
        }

        // Resequence the resulting runs in order before returning to caller.
        _OrderRuns();
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Uses the internal run analysis information (from the analyze step) to map and shape out
//   the glyphs from the fonts. This is effectively a loop of _ShapeGlyphRun. See it for details.
// Arguments:
// - <none> - Uses internal state
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_ShapeGlyphRuns() noexcept
{
    try
    {
        // Shapes all the glyph runs in the layout.
        const auto textLength = gsl::narrow<UINT32>(_text.size());

        // Estimate the maximum number of glyph indices needed to hold a string.
        const auto estimatedGlyphCount = _EstimateGlyphCount(textLength);

        _glyphIndices.resize(estimatedGlyphCount);
        _glyphOffsets.resize(estimatedGlyphCount);
        _glyphAdvances.resize(estimatedGlyphCount);
        _glyphClusters.resize(textLength);

        UINT32 glyphStart = 0;

        // Shape each run separately. This is needed whenever script, locale,
        // or reading direction changes.
        for (UINT32 runIndex = 0; runIndex < _runs.size(); ++runIndex)
        {
            LOG_IF_FAILED(_ShapeGlyphRun(runIndex, glyphStart));
        }

        _glyphIndices.resize(glyphStart);
        _glyphOffsets.resize(glyphStart);
        _glyphAdvances.resize(glyphStart);
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Calculates the following information for any one particular run of text:
//   1. Indices (finding the ID number in each font for each glyph)
//   2. Offsets (the left/right or top/bottom spacing from the baseline origin for each glyph)
//   3. Advances (the width allowed for every glyph)
//   4. Clusters (the bunches of glyphs that represent a particular combined character)
// - A run is defined by the analysis step as a substring of the original text that has similar properties
//   such that it can be processed together as a unit.
// Arguments:
// - runIndex - The ID number of the internal runs array to use while shaping
// - glyphStart - On input, which portion of the internal indices/offsets/etc. arrays to use
//                to write the shaping information.
//              - On output, the position that should be used by the next call as its start position
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_ShapeGlyphRun(const UINT32 runIndex, UINT32& glyphStart) noexcept
{
    try
    {
        // Shapes a single run of text into glyphs.
        // Alternately, you could iteratively interleave shaping and line
        // breaking to reduce the number glyphs held onto at once. It's simpler
        // for this demonstration to just do shaping and line breaking as two
        // separate processes, but realize that this does have the consequence that
        // certain advanced fonts containing line specific features (like Gabriola)
        // will shape as if the line is not broken.

        Run& run = _runs.at(runIndex);
        const auto textStart = run.textStart;
        const auto textLength = run.textLength;
        auto maxGlyphCount = gsl::narrow<UINT32>(_glyphIndices.size() - glyphStart);
        UINT32 actualGlyphCount = 0;

        run.glyphStart = glyphStart;
        run.glyphCount = 0;

        if (textLength == 0)
        {
            return S_FALSE; // Nothing to do..
        }

        // Allocate space for shaping to fill with glyphs and other information,
        // with about as many glyphs as there are text characters. We'll actually
        // need more glyphs than codepoints if they are decomposed into separate
        // glyphs, or fewer glyphs than codepoints if multiple are substituted
        // into a single glyph. In any case, the shaping process will need some
        // room to apply those rules to even make that determination.

        if (textLength > maxGlyphCount)
        {
            maxGlyphCount = _EstimateGlyphCount(textLength);
            const auto totalGlyphsArrayCount = glyphStart + maxGlyphCount;
            _glyphIndices.resize(totalGlyphsArrayCount);
        }

        if (_isEntireTextSimple && !_fontRenderData->DidUserSetFeatures())
        {
            // When the entire text is simple, we can skip GetGlyphs and directly retrieve glyph indices and
            // advances(in font design unit). With the help of font metrics, we can calculate the actual glyph
            // advances without the need of GetGlyphPlacements. This shortcut will significantly reduce the time
            // needed for text analysis.
            DWRITE_FONT_METRICS1 metrics;
            run.fontFace->GetMetrics(&metrics);

            // With simple text, there's only one run. The actual glyph count is the same as textLength.
            _glyphDesignUnitAdvances.resize(textLength);
            _glyphAdvances.resize(textLength);

            auto designUnitsPerEm = metrics.designUnitsPerEm;

            RETURN_IF_FAILED(_fontInUse->GetDesignGlyphAdvances(
                textLength,
                &_glyphIndices.at(glyphStart),
                &_glyphDesignUnitAdvances.at(glyphStart),
                run.isSideways));

            for (size_t i = glyphStart; i < _glyphAdvances.size(); i++)
            {
                _glyphAdvances.at(i) = (float)_glyphDesignUnitAdvances.at(i) / designUnitsPerEm * _formatInUse->GetFontSize() * run.fontScale;
            }

            // Set all the clusters as sequential. In a simple run, we're going 1 to 1.
            // Fill the clusters sequentially from 0 to N-1.
            std::iota(_glyphClusters.begin(), _glyphClusters.end(), gsl::narrow_cast<unsigned short>(0));

            run.glyphCount = textLength;
            glyphStart += textLength;

            return S_OK;
        }

        std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProps(textLength);
        std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps(maxGlyphCount);

        // Get the features to apply to the font
        const auto& features = _fontRenderData->DefaultFontFeatures();
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
        const DWRITE_TYPOGRAPHIC_FEATURES typographicFeatures = { const_cast<DWRITE_FONT_FEATURE*>(features.data()), gsl::narrow<uint32_t>(features.size()) };
        DWRITE_TYPOGRAPHIC_FEATURES const* typographicFeaturesPointer = &typographicFeatures;
        const uint32_t fontFeatureLengths[] = { textLength };

        // Get the glyphs from the text, retrying if needed.

        auto tries = 0;

#pragma warning(suppress : 26485) // so we can pass in the fontFeatureLengths to GetGlyphs without the analyzer complaining
        auto hr = S_OK;
        do
        {
            hr = _fontRenderData->Analyzer()->GetGlyphs(
                &_text.at(textStart),
                textLength,
                run.fontFace.Get(),
                run.isSideways, // isSideways,
                WI_IsFlagSet(run.bidiLevel, 1), // isRightToLeft
                &run.script,
                _localeName.data(),
                (run.isNumberSubstituted) ? _numberSubstitution.Get() : nullptr,
                &typographicFeaturesPointer, // features
                &fontFeatureLengths[0], // featureLengths
                1, // featureCount
                maxGlyphCount, // maxGlyphCount
                &_glyphClusters.at(textStart),
                &textProps.at(0),
                &_glyphIndices.at(glyphStart),
                &glyphProps.at(0),
                &actualGlyphCount);
            tries++;

            if (hr == E_NOT_SUFFICIENT_BUFFER)
            {
                // Try again using a larger buffer.
                maxGlyphCount = _EstimateGlyphCount(maxGlyphCount);
                const auto totalGlyphsArrayCount = glyphStart + maxGlyphCount;

                glyphProps.resize(maxGlyphCount);
                _glyphIndices.resize(totalGlyphsArrayCount);
            }
            else
            {
                break;
            }
        } while (tries < 2); // We'll give it two chances.

        RETURN_IF_FAILED(hr);

        // Get the placement of the all the glyphs.

        _glyphAdvances.resize(std::max(gsl::narrow_cast<size_t>(glyphStart) + gsl::narrow_cast<size_t>(actualGlyphCount), _glyphAdvances.size()));
        _glyphOffsets.resize(std::max(gsl::narrow_cast<size_t>(glyphStart) + gsl::narrow_cast<size_t>(actualGlyphCount), _glyphOffsets.size()));

        const auto fontSizeFormat = _formatInUse->GetFontSize();
        const auto fontSize = fontSizeFormat * run.fontScale;

        hr = _fontRenderData->Analyzer()->GetGlyphPlacements(
            &_text.at(textStart),
            &_glyphClusters.at(textStart),
            &textProps.at(0),
            textLength,
            &_glyphIndices.at(glyphStart),
            &glyphProps.at(0),
            actualGlyphCount,
            run.fontFace.Get(),
            fontSize,
            run.isSideways,
            (run.bidiLevel & 1), // isRightToLeft
            &run.script,
            _localeName.data(),
            &typographicFeaturesPointer, // features
            &fontFeatureLengths[0], // featureLengths
            1, // featureCount
            &_glyphAdvances.at(glyphStart),
            &_glyphOffsets.at(glyphStart));

        RETURN_IF_FAILED(hr);

        // Set the final glyph count of this run and advance the starting glyph.
        run.glyphCount = actualGlyphCount;
        glyphStart += actualGlyphCount;
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Adjusts the glyph information from shaping to fit the layout pattern required
//   for our renderer.
//   This is effectively a loop of _CorrectGlyphRun. See it for details.
// Arguments:
// - <none> - Uses internal state
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_CorrectGlyphRuns() noexcept
{
    try
    {
        // For simple text, there is no need to correct runs.
        if (_isEntireTextSimple)
        {
            return S_OK;
        }

        // Correct each run separately. This is needed whenever script, locale,
        // or reading direction changes.
        for (UINT32 runIndex = 0; runIndex < _runs.size(); ++runIndex)
        {
            LOG_IF_FAILED(_CorrectGlyphRun(runIndex));
        }

        // If scale corrections were needed, we need to split the run.
        for (auto& c : _glyphScaleCorrections)
        {
            // Split after the adjustment first so it
            // takes a copy of all the run properties before we modify them.
            // GH 4665: This is the other half of the potential future perf item.
            //       If glyphs needing the same scale are coalesced, we could
            //       break fewer times and have fewer runs.

            // Example
            // Text:
            // ABCDEFGHIJKLMNOPQRSTUVWXYZ
            // LEN = 26
            // Runs:
            // ^0----^1---------^2-------
            // Scale Factors:
            //  1.0   1.0        1.0
            // (arrows are run begin)
            // 0: IDX = 0, LEN = 6
            // 1: IDX = 6, LEN = 11
            // 2: IDX = 17, LEN = 9

            // From the scale correction... we get
            // IDX = where the scale starts
            // LEN = how long the scale adjustment runs
            // SCALE = the scale factor.

            // We need to split the run so the SCALE factor
            // only applies from IDX to LEN.

            // This is the index after the segment we're splitting.
            const auto afterIndex = c.textIndex + c.textLength;

            // If the after index is still within the text, split the back
            // half off first so we don't apply the scale factor to anything
            // after this glyph/run segment.
            // Example relative to above sample state:
            // Correction says: IDX = 12, LEN = 2, FACTOR = 0.8
            // We must split off first at 14 to leave the existing factor from 14-16.
            // (because the act of splitting copies all properties, we don't want to
            //  adjust the scale factor BEFORE splitting off the existing one.)
            // Text:
            // ABCDEFGHIJKLMNOPQRSTUVWXYZ
            // LEN = 26
            // Runs:
            // ^0----^1----xx^2-^3-------
            // (xx is where we're going to put the correction when all is said and done.
            //  We're adjusting the scale of the "MN" text only.)
            // Scale Factors:
            //  1     1      1   1
            // (arrows are run begin)
            // 0: IDX = 0, LEN = 6
            // 1: IDX = 6, LEN = 8
            // 2: IDX = 14, LEN = 3
            // 3: IDX = 17, LEN = 9
            if (afterIndex < _text.size())
            {
                _SetCurrentRun(afterIndex);
                _SplitCurrentRun(afterIndex);
            }
            // If it's after the text, don't bother. The correction will just apply
            // from the begin point to the end of the text.
            // Example relative to above sample state:
            // Correction says: IDX = 24, LEN = 2
            // Text:
            // ABCDEFGHIJKLMNOPQRSTUVWXYZ
            // LEN = 26
            // Runs:
            // ^0----^1---------^2-----xx
            // xx is where we're going to put the correction when all is said and done.
            // We don't need to split off the back portion because there's nothing after the xx.

            // Now split just this glyph off.
            // Example versus the one above where we did already split the back half off..
            // Correction says: IDX = 12, LEN = 2, FACTOR = 0.8
            // Text:
            // ABCDEFGHIJKLMNOPQRSTUVWXYZ
            // LEN = 26
            // Runs:
            // ^0----^1----^2^3-^4-------
            // (The MN text has been broken into its own run, 2.)
            // Scale Factors:
            //  1     1     1 1  1
            // (arrows are run begin)
            // 0: IDX = 0, LEN = 6
            // 1: IDX = 6, LEN = 6
            // 2: IDX = 12, LEN = 2
            // 2: IDX = 14, LEN = 3
            // 3: IDX = 17, LEN = 9
            _SetCurrentRun(c.textIndex);
            _SplitCurrentRun(c.textIndex);

            // Get the run with the one glyph and adjust the scale.
            auto& run = _GetCurrentRun();
            run.fontScale = c.scale;
            // Correction says: IDX = 12, LEN = 2, FACTOR = 0.8
            // Text:
            // ABCDEFGHIJKLMNOPQRSTUVWXYZ
            // LEN = 26
            // Runs:
            // ^0----^1----^2^3-^4-------
            // (We've now only corrected run 2, selecting only the MN to 0.8)
            // Scale Factors:
            //  1     1    .8 1  1
        }

        // Dump the glyph scale corrections now that we're done with them.
        _glyphScaleCorrections.clear();

        // Order the runs.
        _OrderRuns();
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Adjusts the advances for each glyph in the run so it fits within a fixed-column count of cells.
// Arguments:
// - runIndex - The ID number of the internal runs array to use while shaping.
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_CorrectGlyphRun(const UINT32 runIndex) noexcept
try
{
    const Run& run = _runs.at(runIndex);

    if (run.textLength == 0)
    {
        return S_FALSE; // Nothing to do..
    }

    // We're going to walk through and check for advances that don't match the space that we expect to give out.

    // Glyph Indices represents the number inside the selected font where the glyph image/paths are found.
    // Text represents the original text we gave in.
    // Glyph Clusters represents the map between Text and Glyph Indices.
    //  - There is one Glyph Clusters map column per character of text.
    //  - The value of the cluster at any given position is relative to the 0 index of this run.
    //    (a.k.a. it resets to 0 for every run)
    //  - If multiple Glyph Cluster map values point to the same index, then multiple text chars were used
    //    to create the same glyph cluster.
    //  - The delta between the value from one Glyph Cluster's value and the next one is how many
    //    Glyph Indices are consumed to make that cluster.

    // We're going to walk the map to find what spans of text and glyph indices make one cluster.
    const auto clusterMapBegin = _glyphClusters.cbegin() + run.textStart;
    const auto clusterMapEnd = clusterMapBegin + run.textLength;

    // Walk through every glyph in the run, collect them into clusters, then adjust them to fit in
    // however many columns are expected for display by the text buffer.
#pragma warning(suppress : 26496) // clusterBegin is updated at the bottom of the loop but analysis isn't seeing it.
    for (auto clusterBegin = clusterMapBegin; clusterBegin < clusterMapEnd; /* we will increment this inside the loop*/)
    {
        // One or more glyphs might belong to a single cluster.
        // Consider the following examples:

        // 1.
        // U+00C1 is Á.
        // That is text of length one.
        // A given font might have a complete single glyph for this
        // which will be mapped into the _glyphIndices array.
        // _text[0] = Á
        // _glyphIndices[0] = 153
        // _glyphClusters[0] = 0
        // _glyphClusters[1] = 1
        // The delta between the value of Clusters 0 and 1 is 1.
        // The number of times "0" is specified is once.
        // This means that we've represented one text with one glyph.

        // 2.
        // U+0041 is A and U+0301 is a combining acute accent ´.
        // That is a text length of two.
        // A given font might have two glyphs for this
        // which will be mapped into the _glyphIndices array.
        // _text[0] = A
        // _text[1] = ´ (U+0301, combining acute)
        // _glyphIndices[0] = 153
        // _glyphIndices[1] = 421
        // _glyphClusters[0] = 0
        // _glyphClusters[1] = 0
        // _glyphClusters[2] = 2
        // The delta between the value of Clusters 0/1 and 2 is 2.
        // The number of times "0" is specified is twice.
        // This means that we've represented two text with two glyphs.

        // There are two more scenarios that can occur that get us into
        // NxM territory (N text by M glyphs)

        // 3.
        // U+00C1 is Á.
        // That is text of length one.
        // A given font might represent this as two glyphs
        // which will be mapped into the _glyphIndices array.
        // _text[0] = Á
        // _glyphIndices[0] = 153
        // _glyphIndices[1] = 421
        // _glyphClusters[0] = 0
        // _glyphClusters[1] = 2
        // The delta between the value of Clusters 0 and 1 is 2.
        // The number of times "0" is specified is once.
        // This means that we've represented one text with two glyphs.

        // 4.
        // U+0041 is A and U+0301 is a combining acute accent ´.
        // That is a text length of two.
        // A given font might represent this as one glyph
        // which will be mapped into the _glyphIndices array.
        // _text[0] = A
        // _text[1] = ´ (U+0301, combining acute)
        // _glyphIndices[0] = 984
        // _glyphClusters[0] = 0
        // _glyphClusters[1] = 0
        // _glyphClusters[2] = 1
        // The delta between the value of Clusters 0/1 and 2 is 1.
        // The number of times "0" is specified is twice.
        // This means that we've represented two text with one glyph.

        // Finally, there's one more dimension.
        // Due to supporting a specific coordinate system, the text buffer
        // has told us how many columns it expects the text it gave us to take
        // when displayed.
        // That is stored in _textClusterColumns with one value for each
        // character in the _text array.
        // It isn't aware of how glyphs actually get mapped.
        // So it's giving us a column count in terms of text characters
        // but expecting it to be applied to all the glyphs in the cluster
        // required to represent that text.
        // We'll collect that up and use it at the end to adjust our drawing.

        // Our goal below is to walk through and figure out...
        // A. How many glyphs belong to this cluster?
        // B. Which text characters belong with those glyphs?
        // C. How many columns, in total, were we told we could use
        //    to draw the glyphs?

        // This is the value under the beginning position in the map.
        const auto clusterValue = *clusterBegin;

        // Find the cluster end point inside the map.
        // We want to walk forward in the map until it changes (or we reach the end).
        const auto clusterEnd = std::find_if(clusterBegin, clusterMapEnd, [clusterValue](auto compareVal) -> bool { return clusterValue != compareVal; });

        // The beginning of the text span is just how far the beginning of the cluster is into the map.
        const auto clusterTextBegin = std::distance(_glyphClusters.cbegin(), clusterBegin);

        // The distance from beginning to end is the cluster text length.
        const auto clusterTextLength = std::distance(clusterBegin, clusterEnd);

        // The beginning of the glyph span is just the original cluster value.
        const auto clusterGlyphBegin = clusterValue + run.glyphStart;

        // The difference between the value inside the end iterator and the original value is the glyph length.
        // If the end iterator was off the end of the map, then it's the total run glyph count minus wherever we started.
        const auto clusterGlyphLength = (clusterEnd != clusterMapEnd ? *clusterEnd : run.glyphCount) - clusterValue;

        // Now we can specify the spans within the text-index and glyph-index based vectors
        // that store our drawing metadata.
        // All the text ones run [clusterTextBegin, clusterTextBegin + clusterTextLength)
        // All the cluster ones run [clusterGlyphBegin, clusterGlyphBegin + clusterGlyphLength)

        // Get how many columns we expected the glyph to have.
        const auto columns = base::saturated_cast<UINT16>(std::accumulate(_textClusterColumns.cbegin() + clusterTextBegin,
                                                                          _textClusterColumns.cbegin() + clusterTextBegin + clusterTextLength,
                                                                          0u));

        // Multiply into pixels to get the "advance" we expect this text/glyphs to take when drawn.
        const auto advanceExpected = static_cast<float>(columns * _width);

        // Sum up the advances across the entire cluster to find what the actual value is that we've been told.
        const auto advanceActual = std::accumulate(_glyphAdvances.cbegin() + clusterGlyphBegin,
                                                   _glyphAdvances.cbegin() + clusterGlyphBegin + clusterGlyphLength,
                                                   0.0f);

        // With certain font faces at certain sizes, the advances seem to be slightly more than
        // the pixel grid; Cascadia Code at 13pt (though, 200% scale) had an advance of 10.000001.
        // We don't want anything sub one hundredth of a cell to make us break up runs, because
        // doing so results in suboptimal rendering.
        // If what we expect is bigger than what we have... pad it out.
        if ((advanceExpected - advanceActual) > 0.001f)
        {
            // Get the amount of space we have leftover.
            const auto diff = advanceExpected - advanceActual;

            // Move the X offset (pixels to the right from the left edge) by half the excess space
            // so half of it will be left of the glyph and the other half on the right.
            // Here we need to move every glyph in the cluster.
            std::for_each(_glyphOffsets.begin() + clusterGlyphBegin,
                          _glyphOffsets.begin() + clusterGlyphBegin + clusterGlyphLength,
                          [halfDiff = diff / 2](DWRITE_GLYPH_OFFSET& offset) -> void { offset.advanceOffset += halfDiff; });

            // Set the advance of the final glyph in the set to all excess space not consumed by the first few so
            // we get the perfect width we want.
            _glyphAdvances.at(static_cast<size_t>(clusterGlyphBegin) + clusterGlyphLength - 1) += diff;
        }
        // If what we expect is smaller than what we have... rescale the font size to get a smaller glyph to fit.
        else if ((advanceExpected - advanceActual) < -0.001f)
        {
            const auto scaleProposed = advanceExpected / advanceActual;

            // Store the glyph scale correction for future run breaking
            // GH 4665: In theory, we could also store the length of the new run and coalesce
            //       in case two adjacent glyphs need the same scale factor.
            _glyphScaleCorrections.push_back(ScaleCorrection{
                gsl::narrow<UINT32>(clusterTextBegin),
                gsl::narrow<UINT32>(clusterTextLength),
                scaleProposed });

            // Adjust all relevant advances by the scale factor.
            std::for_each(_glyphAdvances.begin() + clusterGlyphBegin,
                          _glyphAdvances.begin() + clusterGlyphBegin + clusterGlyphLength,
                          [scaleProposed](float& advance) -> void { advance *= scaleProposed; });
        }

        clusterBegin = clusterEnd;
    }

    // Certain fonts, like Batang, contain glyphs for hidden control
    // and formatting characters. So we'll want to explicitly force their
    // advance to zero.
    // I'm leaving this here for future reference, but I don't think we want invisible glyphs for this renderer.
    //if (run.script.shapes & DWRITE_SCRIPT_SHAPES_NO_VISUAL)
    //{
    //    std::fill(_glyphAdvances.begin() + glyphStart,
    //              _glyphAdvances.begin() + glyphStart + actualGlyphCount,
    //              0.0f
    //    );
    //}

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - Takes the analyzed and shaped textual information from the layout process and
//   forwards it into the given renderer in a run-by-run fashion.
// Arguments:
// - clientDrawingContext - Optional pointer to information that the renderer might need
//                          while attempting to graphically place the text onto the screen
// - renderer - The interface to be used for actually putting text onto the screen
// - origin - pixel point of top left corner on final surface for drawing
// Return Value:
// - S_OK or suitable DirectX/DirectWrite/Direct2D result code.
[[nodiscard]] HRESULT CustomTextLayout::_DrawGlyphRuns(_In_opt_ void* clientDrawingContext,
                                                       IDWriteTextRenderer* renderer,
                                                       const D2D_POINT_2F origin) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, renderer);

    try
    {
        // We're going to start from the origin given and walk to the right for each
        // sub-run that was calculated by the layout analysis.
        auto mutableOrigin = origin;

        // Draw each run separately.
        for (auto runIndex = 0; runIndex < gsl::narrow<INT32>(_runs.size()); ++runIndex)
        {
            // Get the run
            const Run& run = _runs.at(runIndex);

            if (!WI_IsFlagSet(run.bidiLevel, 1))
            {
                RETURN_IF_FAILED(_DrawGlyphRun(clientDrawingContext, renderer, mutableOrigin, run));
            }
            // This is the RTL behavior. We will advance to the last contiguous RTL run, draw that,
            // and then keep on going backwards from there, and then move runIndex beyond.
            // Let's say we have runs abcdEFGh, where runs EFG are RTL.
            // Then we will draw them in the order abcdGFEh
            else
            {
                const auto originalRunIndex = runIndex;
                auto lastIndexRTL = runIndex;

                // Step 1: Get to the last contiguous RTL run from here
                while (lastIndexRTL < gsl::narrow<INT32>(_runs.size()) - 1) // only could ever advance if there's something left
                {
                    const Run& nextRun = _runs.at(gsl::narrow_cast<size_t>(lastIndexRTL + 1));
                    if (WI_IsFlagSet(nextRun.bidiLevel, 1))
                    {
                        lastIndexRTL++;
                    }
                    else
                    {
                        break;
                    }
                }

                // Go from the last to the first and draw
                for (runIndex = lastIndexRTL; runIndex >= originalRunIndex; runIndex--)
                {
                    const Run& currentRun = _runs.at(runIndex);
                    RETURN_IF_FAILED(_DrawGlyphRun(clientDrawingContext, renderer, mutableOrigin, currentRun));
                }
                runIndex = lastIndexRTL; // and the for loop will take the increment to the last one
            }
        }
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Draw the given run
// - The origin is updated to be after the run.
// Arguments:
// - clientDrawingContext - Optional pointer to information that the renderer might need
//                          while attempting to graphically place the text onto the screen
// - renderer - The interface to be used for actually putting text onto the screen
// - origin - pixel point of top left corner on final surface for drawing
// - run - the run to be drawn
[[nodiscard]] HRESULT CustomTextLayout::_DrawGlyphRun(_In_opt_ void* clientDrawingContext,
                                                      gsl::not_null<IDWriteTextRenderer*> renderer,
                                                      D2D_POINT_2F& mutableOrigin,
                                                      const Run& run) noexcept
{
    try
    {
        // Prepare the glyph run and description objects by converting our
        // internal storage representation into something that matches DWrite's structures.
        DWRITE_GLYPH_RUN glyphRun;
        glyphRun.bidiLevel = run.bidiLevel;
        glyphRun.fontEmSize = _formatInUse->GetFontSize() * run.fontScale;
        glyphRun.fontFace = run.fontFace.Get();
        glyphRun.glyphAdvances = &_glyphAdvances.at(run.glyphStart);
        glyphRun.glyphCount = run.glyphCount;
        glyphRun.glyphIndices = &_glyphIndices.at(run.glyphStart);
        glyphRun.glyphOffsets = &_glyphOffsets.at(run.glyphStart);
        glyphRun.isSideways = false;

        DWRITE_GLYPH_RUN_DESCRIPTION glyphRunDescription;
        glyphRunDescription.clusterMap = _glyphClusters.data();
        glyphRunDescription.localeName = _localeName.data();
        glyphRunDescription.string = _text.data();
        glyphRunDescription.stringLength = run.textLength;
        glyphRunDescription.textPosition = run.textStart;

        // Calculate the origin for the next run based on the amount of space
        // that would be consumed. We are doing this calculation now, not after,
        // because if the text is RTL then we need to advance immediately, before the
        // write call since DirectX expects the origin to the RIGHT of the text for RTL.
        const auto postOriginX = std::accumulate(_glyphAdvances.begin() + run.glyphStart,
                                                 _glyphAdvances.begin() + run.glyphStart + run.glyphCount,
                                                 mutableOrigin.x);

        // Check for RTL, if it is, apply space adjustment.
        if (WI_IsFlagSet(glyphRun.bidiLevel, 1))
        {
            mutableOrigin.x = postOriginX;
        }

        // Try to draw it
        RETURN_IF_FAILED(renderer->DrawGlyphRun(clientDrawingContext,
                                                mutableOrigin.x,
                                                mutableOrigin.y,
                                                DWRITE_MEASURING_MODE_NATURAL,
                                                &glyphRun,
                                                &glyphRunDescription,
                                                run.drawingEffect.Get()));

        // Either way, we should be at this point by the end of writing this sequence,
        // whether it was LTR or RTL.
        mutableOrigin.x = postOriginX;
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Estimates the maximum number of glyph indices needed to hold a string of
//   a given length.  This is the formula given in the Uniscribe SDK and should
//   cover most cases. Degenerate cases will require a reallocation.
// Arguments:
// - textLength - the number of wchar_ts in the original string
// Return Value:
// - An estimate of how many glyph spaces may be required in the shaping arrays
//   to hold the data from a string of the given length.
[[nodiscard]] constexpr UINT32 CustomTextLayout::_EstimateGlyphCount(const UINT32 textLength) noexcept
{
    // This formula is from https://docs.microsoft.com/en-us/windows/desktop/api/dwrite/nf-dwrite-idwritetextanalyzer-getglyphs
    // and is the recommended formula for estimating buffer size for glyph count.
    return 3 * textLength / 2 + 16;
}

#pragma region IDWriteTextAnalysisSource methods
// Routine Description:
// - Implementation of IDWriteTextAnalysisSource::GetTextAtPosition
// - This method will retrieve a substring of the text in this layout
//   to be used in an analysis step.
// Arguments:
// - textPosition - The index of the first character of the text to retrieve.
// - textString - The pointer to the first character of text at the index requested.
// - textLength - The characters available at/after the textString pointer (string length).
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::GetTextAtPosition(UINT32 textPosition,
                                                                            _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                            _Out_ UINT32* textLength)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, textString);
    RETURN_HR_IF_NULL(E_INVALIDARG, textLength);

    *textString = nullptr;
    *textLength = 0;

    if (textPosition < _text.size())
    {
        *textString = &_text.at(textPosition);
        *textLength = gsl::narrow<UINT32>(_text.size()) - textPosition;
    }

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSource::GetTextBeforePosition
// - This method will retrieve a substring of the text in this layout
//   to be used in an analysis step.
// Arguments:
// - textPosition - The index one after the last character of the text to retrieve.
// - textString - The pointer to the first character of text at the index requested.
// - textLength - The characters available at/after the textString pointer (string length).
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::GetTextBeforePosition(UINT32 textPosition,
                                                                                _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                                _Out_ UINT32* textLength) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, textString);
    RETURN_HR_IF_NULL(E_INVALIDARG, textLength);

    *textString = nullptr;
    *textLength = 0;

    if (textPosition > 0 && textPosition <= _text.size())
    {
        *textString = _text.data();
        *textLength = textPosition;
    }

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSource::GetParagraphReadingDirection
// - This returns the implied reading direction for this block of text (LTR/RTL/etc.)
// Arguments:
// - <none>
// Return Value:
// - The reading direction held for this layout from construction
[[nodiscard]] DWRITE_READING_DIRECTION STDMETHODCALLTYPE CustomTextLayout::GetParagraphReadingDirection() noexcept
{
    return _readingDirection;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSource::GetLocaleName
// - Retrieves the locale name to apply to this text. Sometimes analysis and chosen glyphs vary on locale.
// Arguments:
// - textPosition - The index of the first character in the held string for which layout information is needed
// - textLength - How many characters of the string from the index that the returned locale applies to
// - localeName - Zero terminated string of the locale name.
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::GetLocaleName(UINT32 textPosition,
                                                                        _Out_ UINT32* textLength,
                                                                        _Outptr_result_z_ const WCHAR** localeName) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, textLength);
    RETURN_HR_IF_NULL(E_INVALIDARG, localeName);

    *localeName = _localeName.data();
    *textLength = gsl::narrow<UINT32>(_text.size()) - textPosition;

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSource::GetNumberSubstitution
// - Retrieves the number substitution object name to apply to this text.
// Arguments:
// - textPosition - The index of the first character in the held string for which layout information is needed
// - textLength - How many characters of the string from the index that the returned locale applies to
// - numberSubstitution - Object to use for substituting numbers inside the determined range
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::GetNumberSubstitution(UINT32 textPosition,
                                                                                _Out_ UINT32* textLength,
                                                                                _COM_Outptr_ IDWriteNumberSubstitution** numberSubstitution) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, textLength);
    RETURN_HR_IF_NULL(E_INVALIDARG, numberSubstitution);

    *numberSubstitution = nullptr;
    *textLength = gsl::narrow<UINT32>(_text.size()) - textPosition;

    return S_OK;
}
#pragma endregion

#pragma region IDWriteTextAnalysisSink methods
// Routine Description:
// - Implementation of IDWriteTextAnalysisSink::SetScriptAnalysis
// - Accepts the result of the script analysis computation performed by an IDWriteTextAnalyzer and
//   stores it internally for later shaping and drawing purposes.
// Arguments:
// - textPosition - The index of the first character in the string that the result applies to
// - textLength - How many characters of the string from the index that the result applies to
// - scriptAnalysis - The analysis information for all glyphs starting at position for length.
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::SetScriptAnalysis(UINT32 textPosition,
                                                                            UINT32 textLength,
                                                                            _In_ const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis)
{
    try
    {
        _SetCurrentRun(textPosition);
        _SplitCurrentRun(textPosition);
        while (textLength > 0)
        {
            auto& run = _FetchNextRun(textLength);
            run.script = *scriptAnalysis;
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSink::SetLineBreakpoints
// - Accepts the result of the line breakpoint computation performed by an IDWriteTextAnalyzer and
//   stores it internally for later shaping and drawing purposes.
// Arguments:
// - textPosition - The index of the first character in the string that the result applies to
// - textLength - How many characters of the string from the index that the result applies to
// - scriptAnalysis - The analysis information for all glyphs starting at position for length.
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::SetLineBreakpoints(UINT32 textPosition,
                                                                             UINT32 textLength,
                                                                             _In_reads_(textLength) DWRITE_LINE_BREAKPOINT const* lineBreakpoints)
{
    try
    {
        if (textLength > 0)
        {
            RETURN_HR_IF_NULL(E_INVALIDARG, lineBreakpoints);
            std::copy_n(lineBreakpoints, textLength, _breakpoints.begin() + textPosition);
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSink::SetBidiLevel
// - Accepts the result of the bidirectional analysis computation performed by an IDWriteTextAnalyzer and
//   stores it internally for later shaping and drawing purposes.
// Arguments:
// - textPosition - The index of the first character in the string that the result applies to
// - textLength - How many characters of the string from the index that the result applies to
// - explicitLevel - The analysis information for all glyphs starting at position for length.
// - resolvedLevel - The analysis information for all glyphs starting at position for length.
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::SetBidiLevel(UINT32 textPosition,
                                                                       UINT32 textLength,
                                                                       UINT8 /*explicitLevel*/,
                                                                       UINT8 resolvedLevel)
{
    try
    {
        _SetCurrentRun(textPosition);
        _SplitCurrentRun(textPosition);
        while (textLength > 0)
        {
            auto& run = _FetchNextRun(textLength);
            run.bidiLevel = resolvedLevel;
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextAnalysisSink::SetNumberSubstitution
// - Accepts the result of the number substitution analysis computation performed by an IDWriteTextAnalyzer and
//   stores it internally for later shaping and drawing purposes.
// Arguments:
// - textPosition - The index of the first character in the string that the result applies to
// - textLength - How many characters of the string from the index that the result applies to
// - numberSubstitution - The analysis information for all glyphs starting at position for length.
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::SetNumberSubstitution(UINT32 textPosition,
                                                                                UINT32 textLength,
                                                                                _In_ IDWriteNumberSubstitution* numberSubstitution)
{
    try
    {
        _SetCurrentRun(textPosition);
        _SplitCurrentRun(textPosition);
        while (textLength > 0)
        {
            auto& run = _FetchNextRun(textLength);
            run.isNumberSubstituted = (numberSubstitution != nullptr);
        }
    }
    CATCH_RETURN();

    return S_OK;
}
#pragma endregion

#pragma region internal methods for mimicking text analyzer pattern but for font fallback
// Routine Description:
// - Mimics an IDWriteTextAnalyser but for font fallback calculations.
// Arguments:
// - source - a text analysis source to retrieve substrings of the text to be analyzed
// - textPosition - the index to start the substring operation
// - textLength - the length of the substring operation
// Result:
// - S_OK, STL/GSL errors, or a suitable DirectWrite failure code on font fallback analysis.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_AnalyzeFontFallback(IDWriteTextAnalysisSource* const source,
                                                                               UINT32 textPosition,
                                                                               UINT32 textLength)
{
    try
    {
        // Get the font fallback first
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat1> format1;
        if (FAILED(_formatInUse->QueryInterface(IID_PPV_ARGS(&format1))))
        {
            // If IDWriteTextFormat1 does not exist, return directly as this OS version doesn't have font fallback.
            return S_FALSE;
        }
        RETURN_HR_IF_NULL(E_NOINTERFACE, format1);

        ::Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback;
        RETURN_IF_FAILED(format1->GetFontFallback(&fallback));

        ::Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
        RETURN_IF_FAILED(format1->GetFontCollection(&collection));

        std::wstring familyName;
        familyName.resize(gsl::narrow_cast<size_t>(format1->GetFontFamilyNameLength()) + 1);
        RETURN_IF_FAILED(format1->GetFontFamilyName(familyName.data(), gsl::narrow<UINT32>(familyName.size())));

        const auto weight = format1->GetFontWeight();
        const auto style = format1->GetFontStyle();
        const auto stretch = format1->GetFontStretch();

        if (!fallback)
        {
            fallback = _fontRenderData->SystemFontFallback();
        }

        ::Microsoft::WRL::ComPtr<IDWriteFontFallback1> fallback1;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat3> format3;

        // If the OS supports IDWriteFontFallback1 and IDWriteTextFormat3, we can use the
        // newer MapCharacters to apply axes of variation to the font
        if (!FAILED(_formatInUse->QueryInterface(IID_PPV_ARGS(&format3))) && !FAILED(fallback->QueryInterface(IID_PPV_ARGS(&fallback1))))
        {
            const auto axesVector = _fontRenderData->GetAxisVector(weight, stretch, style, format3.Get());
            // Walk through and analyze the entire string
            while (textLength > 0)
            {
                UINT32 mappedLength = 0;
                ::Microsoft::WRL::ComPtr<IDWriteFontFace5> mappedFont;
                auto scale = 0.0f;

                fallback1->MapCharacters(source,
                                         textPosition,
                                         textLength,
                                         collection.Get(),
                                         familyName.data(),
                                         axesVector.data(),
                                         gsl::narrow<uint32_t>(axesVector.size()),
                                         &mappedLength,
                                         &scale,
                                         &mappedFont);

                RETURN_IF_FAILED(_SetMappedFontFace(textPosition, mappedLength, mappedFont, scale));

                textPosition += mappedLength;
                textLength -= mappedLength;
            }
        }
        else
        {
            // The chunk of code below is very similar to the one above, unfortunately this needs
            // to stay for Win7 compatibility reasons. It is also not possible to combine the two
            // because they call different versions of MapCharacters

            // Walk through and analyze the entire string
            while (textLength > 0)
            {
                UINT32 mappedLength = 0;
                ::Microsoft::WRL::ComPtr<IDWriteFont> mappedFont;
                auto scale = 0.0f;

                fallback->MapCharacters(source,
                                        textPosition,
                                        textLength,
                                        collection.Get(),
                                        familyName.data(),
                                        weight,
                                        style,
                                        stretch,
                                        &mappedLength,
                                        &mappedFont,
                                        &scale);

                RETURN_LAST_ERROR_IF(!mappedFont);
                ::Microsoft::WRL::ComPtr<IDWriteFontFace> face;
                RETURN_IF_FAILED(mappedFont->CreateFontFace(&face));
                RETURN_IF_FAILED(_SetMappedFontFace(textPosition, mappedLength, face, scale));

                textPosition += mappedLength;
                textLength -= mappedLength;
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Mimics an IDWriteTextAnalysisSink but for font fallback calculations with our
//   Analyzer mimic method above.
// Arguments:
// - textPosition - the index to start the substring operation
// - textLength - the length of the substring operation
// - fontFace - the fontFace that applies to the substring range
// - scale - the scale of the font to apply
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_SetMappedFontFace(UINT32 textPosition,
                                                                             UINT32 textLength,
                                                                             const ::Microsoft::WRL::ComPtr<IDWriteFontFace>& fontFace,
                                                                             FLOAT const scale)
{
    try
    {
        _SetCurrentRun(textPosition);
        _SplitCurrentRun(textPosition);
        while (textLength > 0)
        {
            auto& run = _FetchNextRun(textLength);

            if (fontFace != nullptr)
            {
                RETURN_IF_FAILED(fontFace.As(&run.fontFace));
            }
            else
            {
                run.fontFace = _fontInUse;
            }

            // Store the font scale as well.
            run.fontScale = scale;
        }
    }
    CATCH_RETURN();

    return S_OK;
}
#pragma endregion

#pragma region internal methods for mimicking text analyzer to identify and split box drawing regions

// Routine Description:
// - Helper method to detect if something is a box drawing character.
// Arguments:
// - wch - Specific character.
// Return Value:
// - True if box drawing. False otherwise.
static constexpr bool _IsBoxDrawingCharacter(const wchar_t wch)
{
    if (wch >= 0x2500 && wch <= 0x259F)
    {
        return true;
    }

    return false;
}

// Routine Description:
// - Corrects all runs for box drawing characteristics. Splits as it walks, if it must.
//   If there are fallback fonts, this must happen after that's analyzed and after the
//   advances are corrected so we can use the font size scaling factors to determine
//   the appropriate layout heights for the correction scale/translate matrix.
// Arguments:
// - <none> - Operates on all runs then orders them back up.
// Return Value:
// - S_OK, STL/GSL errors, or an E_ABORT from mathematical failures.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_CorrectBoxDrawing() noexcept
try
{
    RETURN_IF_FAILED(_AnalyzeBoxDrawing(this, 0, gsl::narrow<UINT32>(_text.size())));
    _OrderRuns();
    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - An analyzer to walk through the source text and search for runs of box drawing characters.
//   It will segment the text into runs of those characters and mark them for special drawing, if necessary.
// Arguments:
// - source - a text analysis source to retrieve substrings of the text to be analyzed
// - textPosition - the index to start the substring operation
// - textLength - the length of the substring operation
// Result:
// - S_OK, STL/GSL errors, or an E_ABORT from mathematical failures.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_AnalyzeBoxDrawing(const gsl::not_null<IDWriteTextAnalysisSource*> source,
                                                                             UINT32 textPosition,
                                                                             UINT32 textLength)
try
{
    // Walk through and analyze the entire string
    while (textLength > 0)
    {
        // Get the substring of text remaining to analyze.
        const WCHAR* text;
        UINT32 length;
        RETURN_IF_FAILED(source->GetTextAtPosition(textPosition, &text, &length));

        // Put it into a view for iterator convenience.
        const std::wstring_view str(text, length);

        // Find the first box drawing character in the string from the front.
        const auto firstBox = std::find_if(str.cbegin(), str.cend(), _IsBoxDrawingCharacter);

        // If we found no box drawing characters, move on with life.
        if (firstBox == str.cend())
        {
            return S_OK;
        }
        // If we found one, keep looking forward until we find NOT a box drawing character.
        else
        {
            // Find the last box drawing character.
            const auto lastBox = std::find_if(firstBox, str.cend(), [](wchar_t wch) { return !_IsBoxDrawingCharacter(wch); });

            // Skip distance is how far we had to move forward to find a box.
            const auto firstBoxDistance = std::distance(str.cbegin(), firstBox);
            UINT32 skipDistance;
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(firstBoxDistance).AssignIfValid(&skipDistance));

            // Move the position/length of the outside counters up to the part where boxes start.
            textPosition += skipDistance;
            textLength -= skipDistance;

            // Run distance is how many box characters in a row there are.
            const auto runDistance = std::distance(firstBox, lastBox);
            UINT32 mappedLength;
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(runDistance).AssignIfValid(&mappedLength));

            // Split the run and set the box effect on this segment of the run
            RETURN_IF_FAILED(_SetBoxEffect(textPosition, mappedLength));

            // Move us forward for the outer loop to continue scanning after this point.
            textPosition += mappedLength;
            textLength -= mappedLength;
        }
    }

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - A callback to split a run and apply box drawing characteristics to just that sub-run.
// Arguments:
// - textPosition - the index to start the substring operation
// - textLength - the length of the substring operation
// Return Value:
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_SetBoxEffect(UINT32 textPosition,
                                                                        UINT32 textLength)
try
{
    _SetCurrentRun(textPosition);
    _SplitCurrentRun(textPosition);

    while (textLength > 0)
    {
        auto& run = _FetchNextRun(textLength);

        if (run.fontFace == _fontRenderData->DefaultFontFace())
        {
            run.drawingEffect = _fontRenderData->DefaultBoxDrawingEffect();
        }
        else
        {
            ::Microsoft::WRL::ComPtr<IBoxDrawingEffect> eff;
            RETURN_IF_FAILED(DxFontRenderData::s_CalculateBoxEffect(_formatInUse, _width, run.fontFace.Get(), run.fontScale, &eff));

            // store data in the run
            run.drawingEffect = std::move(eff);
        }
    }

    return S_OK;
}
CATCH_RETURN();

#pragma endregion

#pragma region internal Run manipulation functions for storing information from sink callbacks
// Routine Description:
// - Used by the sink setters, this returns a reference to the next run.
//   Position and length are adjusted to now point after the current run
//   being returned.
// Arguments:
// - textLength - The amount of characters for which the next analysis result will apply.
//              - The starting index is implicit based on the currently chosen run.
// Return Value:
// - reference to the run needed to store analysis data
[[nodiscard]] CustomTextLayout::LinkedRun& CustomTextLayout::_FetchNextRun(UINT32& textLength)
{
    const auto originalRunIndex = _runIndex;

    auto& run = _runs.at(originalRunIndex);
    auto runTextLength = run.textLength;

    // Split the tail if needed (the length remaining is less than the
    // current run's size).
    if (textLength < runTextLength)
    {
        runTextLength = textLength; // Limit to what's actually left.
        const auto runTextStart = run.textStart;

        _SplitCurrentRun(runTextStart + runTextLength);
    }
    else
    {
        // Just advance the current run.
        _runIndex = run.nextRunIndex;
    }

    textLength -= runTextLength;

    // Return a reference to the run that was just current.
    // Careful, we have to look it up again as _SplitCurrentRun can resize the array and reshuffle all the reference locations
    return _runs.at(originalRunIndex);
}

// Routine Description:
// - Retrieves the current run according to the internal
//   positioning set by Set/Split Current Run methods.
// Arguments:
// - <none>
// Return Value:
// - Mutable reference of the current run.
[[nodiscard]] CustomTextLayout::LinkedRun& CustomTextLayout::_GetCurrentRun()
{
    return _runs.at(_runIndex);
}

// Routine Description:
// - Move the current run to the given position.
//   Since the analyzers generally return results in a forward manner,
//   this will usually just return early. If not, find the
//   corresponding run for the text position.
// Arguments:
// - textPosition - The index into the original string for which we want to select the corresponding run
// Return Value:
// - <none> - Updates internal state
void CustomTextLayout::_SetCurrentRun(const UINT32 textPosition)
{
    if (_runIndex < _runs.size() && _runs.at(_runIndex).ContainsTextPosition(textPosition))
    {
        return;
    }

    _runIndex = gsl::narrow<UINT32>(
        std::find(_runs.begin(), _runs.end(), textPosition) - _runs.begin());
}

// Routine Description:
// - Splits the current run and adjusts the run values accordingly.
// Arguments:
// - splitPosition - The index into the run where we want to split it into two
// Return Value:
// - <none> - Updates internal state, the back half will be selected after running
void CustomTextLayout::_SplitCurrentRun(const UINT32 splitPosition)
{
    const auto runTextStart = _runs.at(_runIndex).textStart;

    if (splitPosition <= runTextStart)
        return; // no change

    // Grow runs by one.
    const auto totalRuns = _runs.size();
    try
    {
        _runs.resize(totalRuns + 1);
    }
    catch (...)
    {
        return; // Can't increase size. Return same run.
    }

    // Copy the old run to the end.
    auto& frontHalf = _runs.at(_runIndex);
    auto& backHalf = _runs.back();
    backHalf = frontHalf;

    // Adjust runs' text positions and lengths.
    const auto splitPoint = splitPosition - runTextStart;
    backHalf.textStart += splitPoint;
    backHalf.textLength -= splitPoint;
    frontHalf.textLength = splitPoint;
    frontHalf.nextRunIndex = gsl::narrow<UINT32>(totalRuns);
    _runIndex = gsl::narrow<UINT32>(totalRuns);

    // If there is already a glyph mapping in these runs,
    // we need to correct it for the split as well.
    // See also (for NxM):
    // https://social.msdn.microsoft.com/Forums/en-US/993365bc-8689-45ff-a675-c5ed0c011788/dwriteglyphrundescriptionclustermap-explained

    if (frontHalf.glyphCount > 0)
    {
        // Starting from this:
        // TEXT (_text)
        // f  i  ñ  e
        // CLUSTERMAP (_glyphClusters)
        // 0  0  1  3
        // GLYPH INDICES (_glyphIndices)
        // 19 81 23 72
        // With _runs length = 1
        // _runs(0):
        //  - Text Index:   0
        //  - Text Length:  4
        //  - Glyph Index:  0
        //  - Glyph Length: 4
        //
        // If we split at text index = 2 (between i and ñ)...
        // ... then this will be the state after the text splitting above:
        //
        // TEXT (_text)
        // f  i  ñ  e
        // CLUSTERMAP (_glyphClusters)
        // 0  0  1  3
        // GLYPH INDICES (_glyphIndices)
        // 19 81 23 72
        // With _runs length = 2
        // _runs(0):
        //  - Text Index:   0
        //  - Text Length:  2
        //  - Glyph Index:  0
        //  - Glyph Length: 4
        // _runs(1):
        //  - Text Index:   2
        //  - Text Length:  2
        //  - Glyph Index:  0
        //  - Glyph Length: 4
        //
        // Notice that the text index/length values are correct,
        // but we haven't fixed up the glyph index/lengths to match.
        // We need it to say:
        // With _runs length = 2
        // _runs(0):
        //  - Text Index:   0
        //  - Text Length:  2
        //  - Glyph Index:  0
        //  - Glyph Length: 1
        // _runs(1):
        //  - Text Index:   2
        //  - Text Length:  2
        //  - Glyph Index:  1
        //  - Glyph Length: 3
        //
        // Which means that the cluster map value under the beginning
        // of the right-hand text range is our offset to fix all the values.
        // In this case, that's 1 corresponding with the ñ.
        const auto mapOffset = _glyphClusters.at(backHalf.textStart);

        // The front half's glyph start index (position in _glyphIndices)
        // stays the same.

        // The front half's glyph count (items in _glyphIndices to consume)
        // is the offset value as that's now one past the end of the front half.
        // (and count is end index + 1)
        frontHalf.glyphCount = mapOffset;

        // The back half starts at the index that's one past the end of the front
        backHalf.glyphStart += mapOffset;

        // And the back half count (since it was copied from the front half above)
        // now just needs to be subtracted by how many we gave the front half.
        backHalf.glyphCount -= mapOffset;

        // The CLUSTERMAP is also wrong given that it is relative
        // to each run. And now there are two runs so the map
        // value under the ñ and e need to updated to be relative
        // to the text index "2" now instead of the original.
        //
        // For the entire range of the back half, we need to walk through and
        // slide all the glyph mapping values to be relative to the new
        // backHalf.glyphStart, or adjust it by the offset we just set it to.
        const auto updateBegin = _glyphClusters.begin() + backHalf.textStart;
        std::for_each(updateBegin, updateBegin + backHalf.textLength, [mapOffset](UINT16& n) {
            n -= mapOffset;
        });
    }
}

// Routine Description:
// - Takes the linked runs stored in the state variable _runs
//   and ensures that their vector/array indexes are in order in which they're drawn.
// - This is to be used after splitting and reordering them with the split/select functions
//   as those manipulate the runs like a linked list (instead of an ordered array)
//   while splitting to reduce copy overhead and just reorder them when complete with this func.
// Arguments:
// - <none> - Manipulates _runs variable.
// Return Value:
// - <none>
void CustomTextLayout::_OrderRuns()
{
    std::sort(_runs.begin(), _runs.end(), [](auto& a, auto& b) { return a.textStart < b.textStart; });
    for (UINT32 i = 0; i < _runs.size() - 1; ++i)
    {
        til::at(_runs, i).nextRunIndex = i + 1;
    }

    _runs.back().nextRunIndex = 0;
}

#pragma endregion
