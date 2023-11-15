// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "Backend.h"
#include "DWriteTextAnalysis.h"
#include "../../interactivity/win32/CustomWindowMessages.h"

// #### NOTE ####
// This file should only contain methods that are only accessed by the caller of Present() (the "Renderer" class).
// Basically this file poses the "synchronization" point between the concurrently running
// general IRenderEngine API (like the Invalidate*() methods) and the Present() method
// and thus may access both _r and _api.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

#pragma warning(suppress : 26455) // Default constructor may not throw. Declare it 'noexcept' (f.6).
AtlasEngine::AtlasEngine()
{
#ifdef NDEBUG
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(_p.d2dFactory), nullptr, reinterpret_cast<void**>(_p.d2dFactory.addressof())));
#else
    static constexpr D2D1_FACTORY_OPTIONS options{ .debugLevel = D2D1_DEBUG_LEVEL_INFORMATION };
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(_p.d2dFactory), &options, reinterpret_cast<void**>(_p.d2dFactory.addressof())));
#endif

    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(_p.dwriteFactory), reinterpret_cast<::IUnknown**>(_p.dwriteFactory.addressof())));
    _p.dwriteFactory4 = _p.dwriteFactory.try_query<IDWriteFactory4>();

    THROW_IF_FAILED(_p.dwriteFactory->GetSystemFontFallback(_p.systemFontFallback.addressof()));
    _p.systemFontFallback1 = _p.systemFontFallback.try_query<IDWriteFontFallback1>();

    wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
    THROW_IF_FAILED(_p.dwriteFactory->CreateTextAnalyzer(textAnalyzer.addressof()));
    _p.textAnalyzer = textAnalyzer.query<IDWriteTextAnalyzer1>();
}

void AtlasEngine::WaitUntilCanRender() noexcept
{
    if constexpr (ATLAS_DEBUG_RENDER_DELAY)
    {
        Sleep(ATLAS_DEBUG_RENDER_DELAY);
    }
    _waitUntilCanRender();
}

void AtlasEngine::Render(const Render::RenderingPayload& payload)
{
}

void AtlasEngine::_handleSettingsUpdate()
{
    const auto targetChanged = _p.s->target != _api.s->target;
    const auto fontChanged = _p.s->font != _api.s->font;
    const auto cellCountChanged = _p.s->viewportCellCount != _api.s->viewportCellCount;

    _p.s = _api.s;

    if (targetChanged)
    {
        // target->useSoftwareRendering affects the selection of our IDXGIAdapter which requires us to reset _p.dxgi.
        // This will indirectly also recreate the backend, when AtlasEngine::_recreateAdapter() detects this change.
        _p.dxgi = {};
    }
    if (fontChanged)
    {
        _recreateFontDependentResources();
    }
    if (cellCountChanged)
    {
        _recreateCellCountDependentResources();
    }
}

void AtlasEngine::_recreateFontDependentResources()
{
    _api.replacementCharacterFontFace.reset();
    _api.replacementCharacterGlyphIndex = 0;
    _api.replacementCharacterLookedUp = false;

    {
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH];

        if (FAILED(GetUserDefaultLocaleName(&localeName[0], LOCALE_NAME_MAX_LENGTH)))
        {
            memcpy(&localeName[0], L"en-US", 12);
        }

        _api.userLocaleName = std::wstring{ &localeName[0] };
    }

    if (_p.s->font->fontAxisValues.empty())
    {
        for (auto& axes : _api.textFormatAxes)
        {
            axes = {};
        }
    }
    else
    {
        // See AtlasEngine::UpdateFont.
        // It hardcodes indices 0/1/2 in fontAxisValues to the weight/italic/slant axes.
        // If they're -1 they haven't been set by the user and must be filled by us.
        const auto& standardAxes = _p.s->font->fontAxisValues;
        auto fontAxisValues = _p.s->font->fontAxisValues;

        for (size_t i = 0; i < 4; ++i)
        {
            const auto bold = (i & static_cast<size_t>(FontRelevantAttributes::Bold)) != 0;
            const auto italic = (i & static_cast<size_t>(FontRelevantAttributes::Italic)) != 0;
            // The wght axis defaults to the font weight.
            fontAxisValues[0].value = bold ? DWRITE_FONT_WEIGHT_BOLD : (standardAxes[0].value < 0 ? static_cast<f32>(_p.s->font->fontWeight) : standardAxes[0].value);
            // The ital axis defaults to 1 if this is italic and 0 otherwise.
            fontAxisValues[1].value = italic ? 1.0f : (standardAxes[1].value < 0 ? 0.0f : standardAxes[1].value);
            // The slnt axis defaults to -12 if this is italic and 0 otherwise.
            fontAxisValues[2].value = italic ? -12.0f : (standardAxes[2].value < 0 ? 0.0f : standardAxes[2].value);
            _api.textFormatAxes[i] = { fontAxisValues.data(), fontAxisValues.size() };
        }
    }
}

void AtlasEngine::_recreateCellCountDependentResources()
{
    // Let's guess that every cell consists of a surrogate pair.
    const auto projectedTextSize = static_cast<size_t>(_p.s->viewportCellCount.x) * 2;
    // IDWriteTextAnalyzer::GetGlyphs says:
    //   The recommended estimate for the per-glyph output buffers is (3 * textLength / 2 + 16).
    const auto projectedGlyphSize = 3 * projectedTextSize / 2 + 16;

    _api.bufferLine = std::vector<wchar_t>{};
    _api.bufferLine.reserve(projectedTextSize);
    _api.bufferLineColumn.reserve(projectedTextSize + 1);

    _api.analysisResults = std::vector<TextAnalysisSinkResult>{};
    _api.clusterMap = Buffer<u16>{ projectedTextSize };
    _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ projectedTextSize };
    _api.glyphIndices = Buffer<u16>{ projectedGlyphSize };
    _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ projectedGlyphSize };
    _api.glyphAdvances = Buffer<f32>{ projectedGlyphSize };
    _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ projectedGlyphSize };

    _p.unorderedRows = Buffer<ShapedRow>(_p.s->viewportCellCount.y);
    _p.rowsScratch = Buffer<ShapedRow*>(_p.s->viewportCellCount.y);
    _p.rows = Buffer<ShapedRow*>(_p.s->viewportCellCount.y);

    // Our render loop heavily relies on memcpy() which is up to between 1.5x (Intel)
    // and 40x (AMD) faster for allocations with an alignment of 32 or greater.
    // backgroundBitmapStride is a "count" of u32 and not in bytes,
    // so we round up to multiple of 8 because 8 * sizeof(u32) == 32.
    _p.colorBitmapRowStride = (static_cast<size_t>(_p.s->viewportCellCount.x) + 7) & ~7;
    _p.colorBitmapDepthStride = _p.colorBitmapRowStride * _p.s->viewportCellCount.y;
    _p.colorBitmap = Buffer<u32, 32>(_p.colorBitmapDepthStride * 2);
    _p.backgroundBitmap = { _p.colorBitmap.data(), _p.colorBitmapDepthStride };
    _p.foregroundBitmap = { _p.colorBitmap.data() + _p.colorBitmapDepthStride, _p.colorBitmapDepthStride };

    memset(_p.colorBitmap.data(), 0, _p.colorBitmap.size() * sizeof(u32));

    auto it = _p.unorderedRows.data();
    for (auto& r : _p.rows)
    {
        r = it++;
    }
}

void AtlasEngine::_flushBufferLine()
{
    if (_api.bufferLine.empty())
    {
        return;
    }

    const auto cleanup = wil::scope_exit([this]() noexcept {
        _api.bufferLine.clear();
        _api.bufferLineColumn.clear();
    });

    // This would seriously blow us up otherwise.
    Expects(_api.bufferLineColumn.size() == _api.bufferLine.size() + 1);

    auto& row = *_p.rows[_api.lastPaintBufferLineCoord.y];

#pragma warning(suppress : 26494) // Variable 'mappedEnd' is uninitialized. Always initialize an object (type.5).
    for (u32 idx = 0, mappedEnd; idx < _api.bufferLine.size(); idx = mappedEnd)
    {
        u32 mappedLength = 0;
        wil::com_ptr<IDWriteFontFace2> mappedFontFace;
        _mapCharacters(_api.bufferLine.data() + idx, gsl::narrow_cast<u32>(_api.bufferLine.size()) - idx, &mappedLength, mappedFontFace.addressof());
        mappedEnd = idx + mappedLength;

        if (!mappedFontFace)
        {
            _mapReplacementCharacter(idx, mappedEnd, row);
            continue;
        }

        const auto initialIndicesCount = row.glyphIndices.size();

        // GetTextComplexity() returns as many glyph indices as its textLength parameter (here: mappedLength).
        // This block ensures that the buffer has sufficient capacity. It also initializes the glyphProps buffer because it and
        // glyphIndices sort of form a "pair" in the _mapComplex() code and are always simultaneously resized there as well.
        if (mappedLength > _api.glyphIndices.size())
        {
            auto size = _api.glyphIndices.size();
            size = size + (size >> 1);
            size = std::max<size_t>(size, mappedLength);
            Expects(size > _api.glyphIndices.size());
            _api.glyphIndices = Buffer<u16>{ size };
            _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ size };
        }

        if (_api.s->font->fontFeatures.empty())
        {
            // We can reuse idx here, as it'll be reset to "idx = mappedEnd" in the outer loop anyways.
            for (u32 complexityLength = 0; idx < mappedEnd; idx += complexityLength)
            {
                BOOL isTextSimple = FALSE;
                THROW_IF_FAILED(_p.textAnalyzer->GetTextComplexity(_api.bufferLine.data() + idx, mappedEnd - idx, mappedFontFace.get(), &isTextSimple, &complexityLength, _api.glyphIndices.data()));

                if (isTextSimple)
                {
                    const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
                    const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;

                    for (size_t i = 0; i < complexityLength; ++i)
                    {
                        const size_t col1 = _api.bufferLineColumn[idx + i + 0];
                        const size_t col2 = _api.bufferLineColumn[idx + i + 1];
                        const auto glyphAdvance = (col2 - col1) * _p.s->font->cellSize.x;
                        const auto fg = colors[col1 << shift];
                        row.glyphIndices.emplace_back(_api.glyphIndices[i]);
                        row.glyphAdvances.emplace_back(static_cast<f32>(glyphAdvance));
                        row.glyphOffsets.emplace_back();
                        row.colors.emplace_back(fg);
                    }
                }
                else
                {
                    _mapComplex(mappedFontFace.get(), idx, complexityLength, row);
                }
            }
        }
        else
        {
            _mapComplex(mappedFontFace.get(), idx, mappedLength, row);
        }

        const auto indicesCount = row.glyphIndices.size();
        if (indicesCount > initialIndicesCount)
        {
            // IDWriteFontFallback::MapCharacters() isn't just awfully slow,
            // it can also repeatedly return the same font face again and again. :)
            if (row.mappings.empty() || row.mappings.back().fontFace != mappedFontFace)
            {
                row.mappings.emplace_back(std::move(mappedFontFace), gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
            }
            else
            {
                row.mappings.back().glyphsTo = gsl::narrow_cast<u32>(indicesCount);
            }
        }
    }
}

void AtlasEngine::_mapCharacters(const wchar_t* text, const u32 textLength, u32* mappedLength, IDWriteFontFace2** mappedFontFace) const
{
    TextAnalysisSource analysisSource{ _api.userLocaleName.c_str(), text, textLength };
    const auto& textFormatAxis = _api.textFormatAxes[static_cast<size_t>(_api.attributes)];

    // We don't read from scale anyways.
#pragma warning(suppress : 26494) // Variable 'scale' is uninitialized. Always initialize an object (type.5).
    f32 scale;

    if (textFormatAxis)
    {
        THROW_IF_FAILED(_p.systemFontFallback1->MapCharacters(
            /* analysisSource     */ &analysisSource,
            /* textPosition       */ 0,
            /* textLength         */ textLength,
            /* baseFontCollection */ _p.s->font->fontCollection.get(),
            /* baseFamilyName     */ _p.s->font->fontName.c_str(),
            /* fontAxisValues     */ textFormatAxis.data(),
            /* fontAxisValueCount */ gsl::narrow_cast<u32>(textFormatAxis.size()),
            /* mappedLength       */ mappedLength,
            /* scale              */ &scale,
            /* mappedFontFace     */ reinterpret_cast<IDWriteFontFace5**>(mappedFontFace)));
    }
    else
    {
        const auto baseWeight = WI_IsFlagSet(_api.attributes, FontRelevantAttributes::Bold) ? DWRITE_FONT_WEIGHT_BOLD : static_cast<DWRITE_FONT_WEIGHT>(_p.s->font->fontWeight);
        const auto baseStyle = WI_IsFlagSet(_api.attributes, FontRelevantAttributes::Italic) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
        wil::com_ptr<IDWriteFont> font;

        THROW_IF_FAILED(_p.systemFontFallback->MapCharacters(
            /* analysisSource     */ &analysisSource,
            /* textPosition       */ 0,
            /* textLength         */ textLength,
            /* baseFontCollection */ _p.s->font->fontCollection.get(),
            /* baseFamilyName     */ _p.s->font->fontName.c_str(),
            /* baseWeight         */ baseWeight,
            /* baseStyle          */ baseStyle,
            /* baseStretch        */ DWRITE_FONT_STRETCH_NORMAL,
            /* mappedLength       */ mappedLength,
            /* mappedFont         */ font.addressof(),
            /* scale              */ &scale));

        if (font)
        {
            THROW_IF_FAILED(font->CreateFontFace(reinterpret_cast<IDWriteFontFace**>(mappedFontFace)));
        }
    }

    // Oh wow! You found a case where scale isn't 1! I tried every font and none
    // returned something besides 1. I just couldn't figure out why this exists.
    assert(scale == 1);
}

void AtlasEngine::_mapComplex(IDWriteFontFace2* mappedFontFace, u32 idx, u32 length, ShapedRow& row)
{
    _api.analysisResults.clear();

    TextAnalysisSource analysisSource{ _api.userLocaleName.c_str(), _api.bufferLine.data(), gsl::narrow<UINT32>(_api.bufferLine.size()) };
    TextAnalysisSink analysisSink{ _api.analysisResults };
    THROW_IF_FAILED(_p.textAnalyzer->AnalyzeScript(&analysisSource, idx, length, &analysisSink));

    for (const auto& a : _api.analysisResults)
    {
        u32 actualGlyphCount = 0;

#pragma warning(push)
#pragma warning(disable : 26494) // Variable '...' is uninitialized. Always initialize an object (type.5).
        // None of these variables need to be initialized.
        // features/featureRangeLengths are marked _In_reads_opt_(featureRanges).
        // featureRanges is only > 0 when we also initialize all these variables.
        DWRITE_TYPOGRAPHIC_FEATURES feature;
        const DWRITE_TYPOGRAPHIC_FEATURES* features;
        u32 featureRangeLengths;
#pragma warning(pop)
        u32 featureRanges = 0;

        if (!_p.s->font->fontFeatures.empty())
        {
            // Direct2D, why is this mutable?         Why?
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
            feature.features = const_cast<DWRITE_FONT_FEATURE*>(_p.s->font->fontFeatures.data());
            feature.featureCount = gsl::narrow_cast<u32>(_p.s->font->fontFeatures.size());
            features = &feature;
            featureRangeLengths = a.textLength;
            featureRanges = 1;
        }

        if (_api.clusterMap.size() <= a.textLength)
        {
            _api.clusterMap = Buffer<u16>{ static_cast<size_t>(a.textLength) + 1 };
            _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ a.textLength };
        }

        for (auto retry = 0;;)
        {
            const auto hr = _p.textAnalyzer->GetGlyphs(
                /* textString          */ _api.bufferLine.data() + a.textPosition,
                /* textLength          */ a.textLength,
                /* fontFace            */ mappedFontFace,
                /* isSideways          */ false,
                /* isRightToLeft       */ 0,
                /* scriptAnalysis      */ &a.analysis,
                /* localeName          */ _api.userLocaleName.c_str(),
                /* numberSubstitution  */ nullptr,
                /* features            */ &features,
                /* featureRangeLengths */ &featureRangeLengths,
                /* featureRanges       */ featureRanges,
                /* maxGlyphCount       */ gsl::narrow_cast<u32>(_api.glyphIndices.size()),
                /* clusterMap          */ _api.clusterMap.data(),
                /* textProps           */ _api.textProps.data(),
                /* glyphIndices        */ _api.glyphIndices.data(),
                /* glyphProps          */ _api.glyphProps.data(),
                /* actualGlyphCount    */ &actualGlyphCount);

            if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8)
            {
                // Grow factor 1.5x.
                auto size = _api.glyphIndices.size();
                size = size + (size >> 1);
                // Overflow check.
                Expects(size > _api.glyphIndices.size());
                _api.glyphIndices = Buffer<u16>{ size };
                _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ size };
                continue;
            }

            THROW_IF_FAILED(hr);
            break;
        }

        if (_api.glyphAdvances.size() < actualGlyphCount)
        {
            // Grow the buffer by at least 1.5x and at least of `actualGlyphCount` items.
            // The 1.5x growth ensures we don't reallocate every time we need 1 more slot.
            auto size = _api.glyphAdvances.size();
            size = size + (size >> 1);
            size = std::max<size_t>(size, actualGlyphCount);
            _api.glyphAdvances = Buffer<f32>{ size };
            _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ size };
        }

        THROW_IF_FAILED(_p.textAnalyzer->GetGlyphPlacements(
            /* textString          */ _api.bufferLine.data() + a.textPosition,
            /* clusterMap          */ _api.clusterMap.data(),
            /* textProps           */ _api.textProps.data(),
            /* textLength          */ a.textLength,
            /* glyphIndices        */ _api.glyphIndices.data(),
            /* glyphProps          */ _api.glyphProps.data(),
            /* glyphCount          */ actualGlyphCount,
            /* fontFace            */ mappedFontFace,
            /* fontEmSize          */ _p.s->font->fontSize,
            /* isSideways          */ false,
            /* isRightToLeft       */ 0,
            /* scriptAnalysis      */ &a.analysis,
            /* localeName          */ _api.userLocaleName.c_str(),
            /* features            */ &features,
            /* featureRangeLengths */ &featureRangeLengths,
            /* featureRanges       */ featureRanges,
            /* glyphAdvances       */ _api.glyphAdvances.data(),
            /* glyphOffsets        */ _api.glyphOffsets.data()));

        _api.clusterMap[a.textLength] = gsl::narrow_cast<u16>(actualGlyphCount);

        const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
        const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;
        auto prevCluster = _api.clusterMap[0];
        size_t beg = 0;

        for (size_t i = 1; i <= a.textLength; ++i)
        {
            const auto nextCluster = _api.clusterMap[i];
            if (prevCluster == nextCluster)
            {
                continue;
            }

            const size_t col1 = _api.bufferLineColumn[a.textPosition + beg];
            const size_t col2 = _api.bufferLineColumn[a.textPosition + i];
            const auto fg = colors[col1 << shift];

            const auto expectedAdvance = (col2 - col1) * _p.s->font->cellSize.x;
            f32 actualAdvance = 0;
            for (auto j = prevCluster; j < nextCluster; ++j)
            {
                actualAdvance += _api.glyphAdvances[j];
            }
            _api.glyphAdvances[nextCluster - 1] += expectedAdvance - actualAdvance;

            row.colors.insert(row.colors.end(), nextCluster - prevCluster, fg);

            prevCluster = nextCluster;
            beg = i;
        }

        row.glyphIndices.insert(row.glyphIndices.end(), _api.glyphIndices.begin(), _api.glyphIndices.begin() + actualGlyphCount);
        row.glyphAdvances.insert(row.glyphAdvances.end(), _api.glyphAdvances.begin(), _api.glyphAdvances.begin() + actualGlyphCount);
        row.glyphOffsets.insert(row.glyphOffsets.end(), _api.glyphOffsets.begin(), _api.glyphOffsets.begin() + actualGlyphCount);
    }
}

void AtlasEngine::_mapReplacementCharacter(u32 from, u32 to, ShapedRow& row)
{
    if (!_api.replacementCharacterLookedUp)
    {
        bool succeeded = false;

        u32 mappedLength = 0;
        _mapCharacters(L"\uFFFD", 1, &mappedLength, _api.replacementCharacterFontFace.put());

        if (mappedLength == 1)
        {
            static constexpr u32 codepoint = 0xFFFD;
            succeeded = SUCCEEDED(_api.replacementCharacterFontFace->GetGlyphIndicesW(&codepoint, 1, &_api.replacementCharacterGlyphIndex));
        }

        if (!succeeded)
        {
            _api.replacementCharacterFontFace.reset();
            _api.replacementCharacterGlyphIndex = 0;
        }

        _api.replacementCharacterLookedUp = true;
    }

    if (!_api.replacementCharacterFontFace)
    {
        return;
    }

    auto pos1 = from;
    auto pos2 = pos1;
    size_t col1 = _api.bufferLineColumn[from];
    size_t col2 = col1;
    auto initialIndicesCount = row.glyphIndices.size();
    const auto softFontAvailable = !_p.s->font->softFontPattern.empty();
    auto currentlyMappingSoftFont = isSoftFontChar(_api.bufferLine[pos1]);
    const auto shift = gsl::narrow_cast<u8>(row.lineRendition != LineRendition::SingleWidth);
    const auto colors = _p.foregroundBitmap.begin() + _p.colorBitmapRowStride * _api.lastPaintBufferLineCoord.y;

    while (pos2 < to)
    {
        col2 = _api.bufferLineColumn[++pos2];
        if (col1 == col2)
        {
            continue;
        }

        const auto cols = col2 - col1;
        const auto ch = static_cast<u16>(_api.bufferLine[pos1]);
        const auto nowMappingSoftFont = isSoftFontChar(ch);

        row.glyphIndices.emplace_back(nowMappingSoftFont ? ch : _api.replacementCharacterGlyphIndex);
        row.glyphAdvances.emplace_back(static_cast<f32>(cols * _p.s->font->cellSize.x));
        row.glyphOffsets.emplace_back();
        row.colors.emplace_back(colors[col1 << shift]);

        if (currentlyMappingSoftFont != nowMappingSoftFont)
        {
            const auto indicesCount = row.glyphIndices.size();
            const auto fontFace = currentlyMappingSoftFont && softFontAvailable ? nullptr : _api.replacementCharacterFontFace.get();

            if (indicesCount > initialIndicesCount)
            {
                row.mappings.emplace_back(fontFace, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
                initialIndicesCount = indicesCount;
            }
        }

        pos1 = pos2;
        col1 = col2;
        currentlyMappingSoftFont = nowMappingSoftFont;
    }

    {
        const auto indicesCount = row.glyphIndices.size();
        const auto fontFace = currentlyMappingSoftFont && softFontAvailable ? nullptr : _api.replacementCharacterFontFace.get();

        if (indicesCount > initialIndicesCount)
        {
            row.mappings.emplace_back(fontFace, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
        }
    }
}
