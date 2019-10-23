// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CustomTextLayout.h"

#include <wrl.h>
#include <wrl/client.h>
#include <VersionHelpers.h>

using namespace Microsoft::Console::Render;

// Routine Description:
// - Creates a CustomTextLayout object for calculating which glyphs should be placed and where
// Arguments:
// - factory - DirectWrite factory reference in case we need other DirectWrite objects for our layout
// - analyzer - DirectWrite text analyzer from the factory that has been cached at a level above this layout (expensive to create)
// - format - The DirectWrite format object representing the size and other text properties to be applied (by default) to a layout
// - font - The DirectWrite font face to use while calculating layout (by default, will fallback if necessary)
// - clusters - From the backing buffer, the text to be displayed clustered by the columns it should consume.
// - width - The count of pixels available per column (the expected pixel width of every column)
CustomTextLayout::CustomTextLayout(gsl::not_null<IDWriteFactory1*> const factory,
                                   gsl::not_null<IDWriteTextAnalyzer1*> const analyzer,
                                   gsl::not_null<IDWriteTextFormat*> const format,
                                   gsl::not_null<IDWriteFontFace1*> const font,
                                   std::basic_string_view<Cluster> const clusters,
                                   size_t const width) :
    _factory{ factory.get() },
    _analyzer{ analyzer.get() },
    _format{ format.get() },
    _font{ font.get() },
    _localeName{},
    _numberSubstitution{},
    _readingDirection{ DWRITE_READING_DIRECTION_LEFT_TO_RIGHT },
    _runs{},
    _breakpoints{},
    _runIndex{ 0 },
    _width{ width }
{
    // Fetch the locale name out once now from the format
    _localeName.resize(gsl::narrow_cast<size_t>(format->GetLocaleNameLength()) + 1); // +1 for null
    THROW_IF_FAILED(format->GetLocaleName(_localeName.data(), gsl::narrow<UINT32>(_localeName.size())));

    for (const auto& cluster : clusters)
    {
        const auto cols = gsl::narrow<UINT16>(cluster.GetColumns());
        _textClusterColumns.push_back(cols);
        _text += cluster.GetText();
    }
}

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
{
    RETURN_IF_FAILED(_AnalyzeRuns());
    RETURN_IF_FAILED(_ShapeGlyphRuns());
    RETURN_IF_FAILED(_CorrectGlyphRuns());
    RETURN_IF_FAILED(_DrawGlyphRuns(clientDrawingContext, renderer, { originX, originY }));

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
        initialRun.nextRunIndex = 0;
        initialRun.textStart = 0;
        initialRun.textLength = textLength;
        initialRun.bidiLevel = (_readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);

        // Allocate enough room to have one breakpoint per code unit.
        _breakpoints.resize(_text.size());

        BOOL isTextSimple = FALSE;
        UINT32 uiLengthRead = 0;
        RETURN_IF_FAILED(_analyzer->GetTextComplexity(_text.c_str(), textLength, _font.Get(), &isTextSimple, &uiLengthRead, NULL));

        if (!(isTextSimple && uiLengthRead == _text.size()))
        {
            // Call each of the analyzers in sequence, recording their results.
            RETURN_IF_FAILED(_analyzer->AnalyzeLineBreakpoints(this, 0, textLength, this));
            RETURN_IF_FAILED(_analyzer->AnalyzeBidi(this, 0, textLength, this));
            RETURN_IF_FAILED(_analyzer->AnalyzeScript(this, 0, textLength, this));
            RETURN_IF_FAILED(_analyzer->AnalyzeNumberSubstitution(this, 0, textLength, this));
            // Perform our custom font fallback analyzer that mimics the pattern of the real analyzers.
            RETURN_IF_FAILED(_AnalyzeFontFallback(this, 0, textLength));
        }

        // Ensure that a font face is attached to every run
        for (auto& run : _runs)
        {
            if (!run.fontFace)
            {
                run.fontFace = _font;
            }
        }

        // Resequence the resulting runs in order before returning to caller.
        const size_t totalRuns = _runs.size();
        std::vector<LinkedRun> runs;
        runs.resize(totalRuns);

        UINT32 nextRunIndex = 0;
        for (size_t i = 0; i < totalRuns; ++i)
        {
            runs.at(i) = _runs.at(nextRunIndex);
            nextRunIndex = _runs.at(nextRunIndex).nextRunIndex;
        }

        _runs.swap(runs);
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
        const UINT32 estimatedGlyphCount = _EstimateGlyphCount(textLength);

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
        // for this demostration to just do shaping and line breaking as two
        // separate processes, but realize that this does have the consequence that
        // certain advanced fonts containing line specific features (like Gabriola)
        // will shape as if the line is not broken.

        Run& run = _runs.at(runIndex);
        const UINT32 textStart = run.textStart;
        const UINT32 textLength = run.textLength;
        UINT32 maxGlyphCount = gsl::narrow<UINT32>(_glyphIndices.size() - glyphStart);
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
        // room to apply those rules to even make that determintation.

        if (textLength > maxGlyphCount)
        {
            maxGlyphCount = _EstimateGlyphCount(textLength);
            const UINT32 totalGlyphsArrayCount = glyphStart + maxGlyphCount;
            _glyphIndices.resize(totalGlyphsArrayCount);
        }

        std::vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProps(textLength);
        std::vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProps(maxGlyphCount);

        // Get the glyphs from the text, retrying if needed.

        int tries = 0;

        HRESULT hr = S_OK;
        do
        {
            hr = _analyzer->GetGlyphs(
                &_text.at(textStart),
                textLength,
                run.fontFace.Get(),
                run.isSideways, // isSideways,
                WI_IsFlagSet(run.bidiLevel, 1), // isRightToLeft
                &run.script,
                _localeName.data(),
                (run.isNumberSubstituted) ? _numberSubstitution.Get() : nullptr,
                nullptr, // features
                nullptr, // featureLengths
                0, // featureCount
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
                const UINT32 totalGlyphsArrayCount = glyphStart + maxGlyphCount;

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

        const auto fontSizeFormat = _format->GetFontSize();
        const auto fontSize = fontSizeFormat * run.fontScale;

        hr = _analyzer->GetGlyphPlacements(
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
            NULL, // features
            NULL, // featureRangeLengths
            0, // featureRanges
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
        // Correct each run separately. This is needed whenever script, locale,
        // or reading direction changes.
        for (UINT32 runIndex = 0; runIndex < _runs.size(); ++runIndex)
        {
            LOG_IF_FAILED(_CorrectGlyphRun(runIndex));
        }
    }
    CATCH_RETURN();
    return S_OK;
}

// Routine Description:
// - Adjusts the advances for each glyph in the run so it fits within a fixed-column count of cells.
// Arguments:
// - runIndex - The ID number of the internal runs array to use while shaping
// Return Value:
// - S_OK or suitable DirectWrite or STL error code
[[nodiscard]] HRESULT CustomTextLayout::_CorrectGlyphRun(const UINT32 runIndex) noexcept
{
    try
    {
        Run& run = _runs.at(runIndex);

        if (run.textLength == 0)
        {
            return S_FALSE; // Nothing to do..
        }

        // We're going to walk through and check for advances that don't match the space that we expect to give out.

        DWRITE_FONT_METRICS1 metrics;
        run.fontFace->GetMetrics(&metrics);

        // Walk through advances and space out characters that are too small to consume their box.
        for (auto i = run.glyphStart; i < (run.glyphStart + run.glyphCount); i++)
        {
            // Advance is how wide in pixels the glyph is
            auto& advance = _glyphAdvances.at(i);

            // Offsets is how far to move the origin (in pixels) from where it is
            auto& offset = _glyphOffsets.at(i);

            // Get how many columns we expected the glyph to have and mutiply into pixels.
            const auto columns = _textClusterColumns.at(i);
            const auto advanceExpected = static_cast<float>(columns * _width);

            // If what we expect is bigger than what we have... pad it out.
            if (advanceExpected > advance)
            {
                // Get the amount of space we have leftover.
                const auto diff = advanceExpected - advance;

                // Move the X offset (pixels to the right from the left edge) by half the excess space
                // so half of it will be left of the glyph and the other half on the right.
                offset.advanceOffset += diff / 2;

                // Set the advance to the perfect width we want.
                advance = advanceExpected;
            }
            // If what we expect is smaller than what we have... rescale the font size to get a smaller glyph to fit.
            else if (advanceExpected < advance)
            {
                // We need to retrieve the design information for this specific glyph so we can figure out the appropriate
                // height proportional to the width that we desire.
                INT32 advanceInDesignUnits;
                RETURN_IF_FAILED(run.fontFace->GetDesignGlyphAdvances(1, &_glyphIndices.at(i), &advanceInDesignUnits));

                // When things are drawn, we want the font size (as specified in the base font in the original format)
                // to be scaled by some factor.
                // i.e. if the original font size was 16, we might want to draw this glyph with a 15.2 size font so
                // the width (and height) of the glyph will shrink to fit the monospace cell box.

                // This pattern is copied from the DxRenderer's algorithm for figuring out the font height for a specific width
                // and was advised by the DirectWrite team.
                const float widthAdvance = static_cast<float>(advanceInDesignUnits) / metrics.designUnitsPerEm;
                const auto fontSizeWant = advanceExpected / widthAdvance;
                run.fontScale = fontSizeWant / _format->GetFontSize();

                // Set the advance to the perfect width that we want.
                advance = advanceExpected;
            }
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
    }
    CATCH_RETURN();
    return S_OK;
}

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
        for (UINT32 runIndex = 0; runIndex < _runs.size(); ++runIndex)
        {
            // Get the run
            const Run& run = _runs.at(runIndex);

            // Prepare the glyph run and description objects by converting our
            // internal storage representation into something that matches DWrite's structures.
            DWRITE_GLYPH_RUN glyphRun;
            glyphRun.bidiLevel = run.bidiLevel;
            glyphRun.fontEmSize = _format->GetFontSize() * run.fontScale;
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
                                                    nullptr));

            // Either way, we should be at this point by the end of writing this sequence,
            // whether it was LTR or RTL.
            mutableOrigin.x = postOriginX;
        }
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
                                                                        _Outptr_result_z_ WCHAR const** localeName) noexcept
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
                                                                            _In_ DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis)
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

#pragma region internal methods for mimicing text analyzer pattern but for font fallback
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
        if (FAILED(_format.As(&format1)))
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
            ::Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
            RETURN_IF_FAILED(_factory.As(&factory2));
            factory2->GetSystemFontFallback(&fallback);
        }

        // Walk through and analyze the entire string
        while (textLength > 0)
        {
            UINT32 mappedLength = 0;
            ::Microsoft::WRL::ComPtr<IDWriteFont> mappedFont;
            FLOAT scale = 0.0f;

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

            RETURN_IF_FAILED(_SetMappedFont(textPosition, mappedLength, mappedFont.Get(), scale));

            textPosition += mappedLength;
            textLength -= mappedLength;
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
// - font - the font that applies to the substring range
// - scale - the scale of the font to apply
// - S_OK or appropriate STL/GSL failure code.
[[nodiscard]] HRESULT STDMETHODCALLTYPE CustomTextLayout::_SetMappedFont(UINT32 textPosition,
                                                                         UINT32 textLength,
                                                                         _In_ IDWriteFont* const font,
                                                                         FLOAT const scale)
{
    try
    {
        _SetCurrentRun(textPosition);
        _SplitCurrentRun(textPosition);
        while (textLength > 0)
        {
            auto& run = _FetchNextRun(textLength);

            if (font != nullptr)
            {
                // Get font face from font metadata
                ::Microsoft::WRL::ComPtr<IDWriteFontFace> face;
                RETURN_IF_FAILED(font->CreateFontFace(&face));

                // QI for Face5 interface from base face interface, store into run
                RETURN_IF_FAILED(face.As(&run.fontFace));
            }
            else
            {
                run.fontFace = _font;
            }

            // Store the font scale as well.
            run.fontScale = scale;
        }
    }
    CATCH_RETURN();

    return S_OK;
}

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
    UINT32 runTextLength = run.textLength;

    // Split the tail if needed (the length remaining is less than the
    // current run's size).
    if (textLength < runTextLength)
    {
        runTextLength = textLength; // Limit to what's actually left.
        const UINT32 runTextStart = run.textStart;

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
    const UINT32 runTextStart = _runs.at(_runIndex).textStart;

    if (splitPosition <= runTextStart)
        return; // no change

    // Grow runs by one.
    const size_t totalRuns = _runs.size();
    try
    {
        _runs.resize(totalRuns + 1);
    }
    catch (...)
    {
        return; // Can't increase size. Return same run.
    }

    // Copy the old run to the end.
    LinkedRun& frontHalf = _runs.at(_runIndex);
    LinkedRun& backHalf = _runs.back();
    backHalf = frontHalf;

    // Adjust runs' text positions and lengths.
    const UINT32 splitPoint = splitPosition - runTextStart;
    backHalf.textStart += splitPoint;
    backHalf.textLength -= splitPoint;
    frontHalf.textLength = splitPoint;
    frontHalf.nextRunIndex = gsl::narrow<UINT32>(totalRuns);
    _runIndex = gsl::narrow<UINT32>(totalRuns);
}
#pragma endregion
