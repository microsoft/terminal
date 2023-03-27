// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "Backend.h"
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

#pragma region IRenderEngine

// StartPaint() is called while the console buffer lock is being held.
// --> Put as little in here as possible.
[[nodiscard]] HRESULT AtlasEngine::StartPaint() noexcept
try
{
    if (const auto hwnd = _api.s->target->hwnd)
    {
        RECT rect;
        LOG_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd, &rect));
        std::ignore = SetWindowSize({ rect.right - rect.left, rect.bottom - rect.top });

        if (_api.invalidatedTitle)
        {
            LOG_IF_WIN32_BOOL_FALSE(PostMessageW(hwnd, CM_UPDATE_TITLE, 0, 0));
            _api.invalidatedTitle = false;
        }
    }

    if (_p.s != _api.s)
    {
        _handleSettingsUpdate();
    }

    if constexpr (ATLAS_DEBUG_DISABLE_PARTIAL_INVALIDATION)
    {
        _api.invalidatedRows = invalidatedRowsAll;
        _api.scrollOffset = 0;
    }

    // Clamp invalidation rects into valid value ranges.
    {
        _api.invalidatedCursorArea.left = std::min(_api.invalidatedCursorArea.left, _p.s->cellCount.x);
        _api.invalidatedCursorArea.top = std::min(_api.invalidatedCursorArea.top, _p.s->cellCount.y);
        _api.invalidatedCursorArea.right = clamp(_api.invalidatedCursorArea.right, _api.invalidatedCursorArea.left, _p.s->cellCount.x);
        _api.invalidatedCursorArea.bottom = clamp(_api.invalidatedCursorArea.bottom, _api.invalidatedCursorArea.top, _p.s->cellCount.y);
    }
    {
        _api.invalidatedRows.x = std::min(_api.invalidatedRows.x, _p.s->cellCount.y);
        _api.invalidatedRows.y = clamp(_api.invalidatedRows.y, _api.invalidatedRows.x, _p.s->cellCount.y);
    }
    {
        const auto limit = gsl::narrow_cast<i16>(_p.s->cellCount.y & 0x7fff);
        _api.scrollOffset = gsl::narrow_cast<i16>(clamp<int>(_api.scrollOffset, -limit, limit));
    }

    // Scroll the buffer by the given offset and mark the newly uncovered rows as "invalid".
    if (const auto offset = _api.scrollOffset)
    {
        const auto nothingInvalid = _api.invalidatedRows.x == _api.invalidatedRows.y;

        if (offset < 0)
        {
            // scrollOffset/offset = -1
            // +----------+    +----------+
            // |          |    | xxxxxxxxx|
            // | xxxxxxxxx| -> |xxxxxxx   |
            // |xxxxxxx   |    |          |
            // +----------+    +----------+
            const u16 begRow = _p.s->cellCount.y + offset;
            _api.invalidatedRows.x = nothingInvalid ? begRow : std::min(_api.invalidatedRows.x, begRow);
            _api.invalidatedRows.y = _p.s->cellCount.y;

            const auto dst = std::copy_n(_p.rows.begin() - offset, _p.rows.size() + offset, _p.rowsScratch.begin());
            std::copy_n(_p.rows.begin(), -offset, dst);
        }
        else
        {
            // scrollOffset/offset = 1
            // +----------+    +----------+
            // | xxxxxxxxx|    |          |
            // |xxxxxxx   | -> | xxxxxxxxx|
            // |          |    |xxxxxxx   |
            // +----------+    +----------+
            const u16 endRow = offset;
            _api.invalidatedRows.x = 0;
            _api.invalidatedRows.y = nothingInvalid ? endRow : std::max(_api.invalidatedRows.y, endRow);

            const auto dst = std::copy_n(_p.rows.end() - offset, offset, _p.rowsScratch.begin());
            std::copy_n(_p.rows.begin(), _p.rows.size() - offset, dst);
        }

        std::swap(_p.rows, _p.rowsScratch);

        // Scrolling the background bitmap is a lot easier because we can rely on memmove which works
        // with both forwards and backwards copying. It's a mystery why the STL doesn't have this.
        {
            const auto width = _p.s->cellCount.x;
            const auto beg = _p.backgroundBitmap.begin();
            const auto end = _p.backgroundBitmap.end();
            const auto src = beg - std::min<ptrdiff_t>(0, offset) * width;
            const auto dst = beg + std::max<ptrdiff_t>(0, offset) * width;
            const auto count = end - std::max(src, dst);
            assert(dst >= beg && dst + count <= end);
            assert(src >= beg && src + count <= end);
            memmove(dst, src, count * sizeof(u32));
            _p.backgroundBitmapGeneration.bump();
        }
    }

    _api.dirtyRect = {
        0,
        _api.invalidatedRows.x,
        _p.s->cellCount.x,
        _api.invalidatedRows.y,
    };

    _p.dirtyRectInPx = {
        til::CoordTypeMax,
        til::CoordTypeMax,
        til::CoordTypeMin,
        til::CoordTypeMin,
    };
    _p.invalidatedRows = _api.invalidatedRows;
    _p.cursorRect = {};
    _p.scrollOffset = _api.scrollOffset;

    if (_api.invalidatedRows.x != _api.invalidatedRows.y)
    {
        const auto deltaPx = _api.scrollOffset * _p.s->font->cellSize.y;
        const til::CoordType targetSizeX = _p.s->targetSize.x;
        const til::CoordType targetSizeY = _p.s->targetSize.y;
        u16 y = 0;

        _p.dirtyRectInPx.left = 0;
        _p.dirtyRectInPx.top = _api.invalidatedRows.x * _p.s->font->cellSize.y;
        _p.dirtyRectInPx.right = targetSizeX;
        _p.dirtyRectInPx.bottom = _api.invalidatedRows.y * _p.s->font->cellSize.y;

        for (const auto r : _p.rows)
        {
            r->dirtyTop += deltaPx;
            r->dirtyBottom += deltaPx;

            if (y >= _api.invalidatedRows.x && y < _api.invalidatedRows.y)
            {
                const auto clampedTop = clamp(r->dirtyTop, 0, targetSizeY);
                const auto clampedBottom = clamp(r->dirtyBottom, 0, targetSizeY);
                if (clampedTop != clampedBottom)
                {
                    _p.dirtyRectInPx.top = std::min(_p.dirtyRectInPx.top, clampedTop);
                    _p.dirtyRectInPx.bottom = std::max(_p.dirtyRectInPx.bottom, clampedBottom);
                }

                r->clear(y, _p.s->font->cellSize.y);
            }

            ++y;
        }

        // I feel a little bit like this is a hack, but I'm not sure how to better express this.
        // This ensures that we end up calling Present1() without dirty rects if the swap chain is
        // recreated/resized, because DXGI requires you to then call Present1() without dirty rects.
        if (_api.invalidatedRows.x == 0 && _api.invalidatedRows.y == _p.s->cellCount.y)
        {
            _p.dirtyRectInPx.top = 0;
            _p.dirtyRectInPx.bottom = targetSizeY;
        }
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::EndPaint() noexcept
try
{
    _flushBufferLine();

    _api.invalidatedCursorArea = invalidatedAreaNone;
    _api.invalidatedRows = invalidatedRowsNone;
    _api.scrollOffset = 0;
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);
    *pForcePaint = false;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::ScrollFrame() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PrepareRenderInfo(const RenderFrameInfo& info) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::ResetLineTransform() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PrepareLineTransform(const LineRendition lineRendition, const til::CoordType targetRow, const til::CoordType viewportLeft) noexcept
{
    const auto y = gsl::narrow_cast<u16>(clamp<til::CoordType>(targetRow, 0, _p.s->cellCount.y));
    _p.rows[y]->lineRendition = static_cast<FontRendition>(lineRendition);
    _api.lineRendition = lineRendition;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PaintBackground() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PaintBufferLine(std::span<const Cluster> clusters, til::point coord, const bool fTrimLeft, const bool lineWrapped) noexcept
try
{
    const auto y = gsl::narrow_cast<u16>(clamp<int>(coord.y, 0, _p.s->cellCount.y));

    if (_api.lastPaintBufferLineCoord.y != y)
    {
        _flushBufferLine();
    }

    // _api.bufferLineColumn contains 1 more item than _api.bufferLine, as it represents the
    // past-the-end index. It'll get appended again later once we built our new _api.bufferLine.
    if (!_api.bufferLineColumn.empty())
    {
        _api.bufferLineColumn.pop_back();
    }

    const auto x = gsl::narrow_cast<u16>(clamp<int>(coord.x, 0, _p.s->cellCount.x));
    auto column = x;

    // Due to the current IRenderEngine interface (that wasn't refactored yet) we need to assemble
    // the current buffer line first as the remaining function operates on whole lines of text.
    {
        for (const auto& cluster : clusters)
        {
            for (const auto& ch : cluster.GetText())
            {
                _api.bufferLine.emplace_back(ch);
                _api.bufferLineColumn.emplace_back(column);
            }

            column += gsl::narrow_cast<u16>(cluster.GetColumns());
        }

        _api.bufferLineColumn.emplace_back(column);
    }

    {
        std::fill(_api.colorsForeground.begin() + x, _api.colorsForeground.begin() + column, _api.currentColor.x);
    }

    {
        const auto shift = _api.lineRendition >= LineRendition::DoubleWidth ? 1 : 0;
        const auto backgroundRow = _p.backgroundBitmap.begin() + static_cast<size_t>(y) * _p.s->cellCount.x;
        auto it = backgroundRow + x;
        const auto end = backgroundRow + (static_cast<size_t>(column) << shift);
        const auto bg = _api.currentColor.y;

        for (; it != end; ++it)
        {
            if (*it != bg)
            {
                _p.backgroundBitmapGeneration.bump();
                std::fill(it, end, bg);
                break;
            }
        }
    }

    _api.lastPaintBufferLineCoord = { x, y };
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintBufferGridLines(const GridLineSet lines, const COLORREF color, const size_t cchLine, const til::point coordTarget) noexcept
try
{
    const auto shift = _api.lineRendition >= LineRendition::DoubleWidth ? 1 : 0;
    const auto y = gsl::narrow_cast<u16>(clamp<til::CoordType>(coordTarget.y, 0, _p.s->cellCount.y));
    const auto from = gsl::narrow_cast<u16>(clamp<til::CoordType>(coordTarget.x << shift, 0, _p.s->cellCount.x - 1));
    const auto to = gsl::narrow_cast<u16>(clamp<size_t>((coordTarget.x + cchLine) << shift, from, _p.s->cellCount.x));
    const auto fg = gsl::narrow_cast<u32>(color) | 0xff000000;
    _p.rows[y]->gridLineRanges.emplace_back(lines, fg, from, to);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintSelection(const til::rect& rect) noexcept
try
{
    // Unfortunately there's no step after Renderer::_PaintBufferOutput that
    // would inform us that it's done with the last AtlasEngine::PaintBufferLine.
    // As such we got to call _flushBufferLine() here just to be sure.
    _flushBufferLine();

    const auto y = gsl::narrow_cast<u16>(clamp<til::CoordType>(rect.top, 0, _p.s->cellCount.y));
    const auto from = gsl::narrow_cast<u16>(clamp<til::CoordType>(rect.left, 0, _p.s->cellCount.x - 1));
    const auto to = gsl::narrow_cast<u16>(clamp<til::CoordType>(rect.right, from, _p.s->cellCount.x));

    auto& row = *_p.rows[y];
    row.selectionFrom = from;
    row.selectionTo = to;

    _p.dirtyRectInPx.left = std::min(_p.dirtyRectInPx.left, from * _p.s->font->cellSize.x);
    _p.dirtyRectInPx.top = std::min(_p.dirtyRectInPx.top, y * _p.s->font->cellSize.y);
    _p.dirtyRectInPx.right = std::max(_p.dirtyRectInPx.right, to * _p.s->font->cellSize.x);
    _p.dirtyRectInPx.bottom = std::max(_p.dirtyRectInPx.bottom, _p.dirtyRectInPx.top + _p.s->font->cellSize.y);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintCursor(const CursorOptions& options) noexcept
try
{
    // Unfortunately there's no step after Renderer::_PaintBufferOutput that
    // would inform us that it's done with the last AtlasEngine::PaintBufferLine.
    // As such we got to call _flushBufferLine() here just to be sure.
    _flushBufferLine();

    {
        const CursorSettings cachedOptions{
            .cursorColor = gsl::narrow_cast<u32>(options.fUseColor ? options.cursorColor | 0xff000000 : INVALID_COLOR),
            .cursorType = gsl::narrow_cast<u16>(options.cursorType),
            .heightPercentage = gsl::narrow_cast<u16>(options.ulCursorHeightPercent),
        };
        if (*_api.s->cursor != cachedOptions)
        {
            *_api.s.write()->cursor.write() = cachedOptions;
            *_p.s.write()->cursor.write() = cachedOptions;
        }
    }

    // Clear the previous cursor
    if (const auto r = _api.invalidatedCursorArea; r.non_empty())
    {
        _p.dirtyRectInPx.left = std::min(_p.dirtyRectInPx.left, r.left * _p.s->font->cellSize.x);
        _p.dirtyRectInPx.top = std::min(_p.dirtyRectInPx.top, r.top * _p.s->font->cellSize.y);
        _p.dirtyRectInPx.right = std::max(_p.dirtyRectInPx.right, r.right * _p.s->font->cellSize.x);
        _p.dirtyRectInPx.bottom = std::max(_p.dirtyRectInPx.bottom, r.bottom * _p.s->font->cellSize.y);
    }

    if (options.isOn)
    {
        const auto point = options.coordCursor;
        // TODO: options.coordCursor can contain invalid out of bounds coordinates when
        // the window is being resized and the cursor is on the last line of the viewport.
        const auto x = gsl::narrow_cast<u16>(clamp(point.x, 0, _p.s->cellCount.x - 1));
        const auto y = gsl::narrow_cast<u16>(clamp(point.y, 0, _p.s->cellCount.y - 1));
        const auto cursorWidth = 1 + (options.fIsDoubleWidth & (options.cursorType != CursorType::VerticalBar));
        const auto right = gsl::narrow_cast<u16>(clamp(x + cursorWidth, 0, _p.s->cellCount.x - 0));
        const auto bottom = gsl::narrow_cast<u16>(y + 1);
        _p.cursorRect = { x, y, right, bottom };
        _p.dirtyRectInPx.left = std::min(_p.dirtyRectInPx.left, x * _p.s->font->cellSize.x);
        _p.dirtyRectInPx.top = std::min(_p.dirtyRectInPx.top, y * _p.s->font->cellSize.y);
        _p.dirtyRectInPx.right = std::max(_p.dirtyRectInPx.right, right * _p.s->font->cellSize.x);
        _p.dirtyRectInPx.bottom = std::max(_p.dirtyRectInPx.bottom, bottom * _p.s->font->cellSize.y);
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, const gsl::not_null<IRenderData*> /*pData*/, const bool usingSoftFont, const bool isSettingDefaultBrushes) noexcept
try
{
    auto [fg, bg] = renderSettings.GetAttributeColorsWithAlpha(textAttributes);
    fg |= 0xff000000;
    bg |= _api.backgroundOpaqueMixin;

    if (!isSettingDefaultBrushes)
    {
        const u32x2 newColors{ gsl::narrow_cast<u32>(fg), gsl::narrow_cast<u32>(bg) };

        auto attributes = FontRelevantAttributes::None;
        WI_SetFlagIf(attributes, FontRelevantAttributes::Bold, textAttributes.IsIntense() && renderSettings.GetRenderMode(RenderSettings::Mode::IntenseIsBold));
        WI_SetFlagIf(attributes, FontRelevantAttributes::Italic, textAttributes.IsItalic());

        if (_api.attributes != attributes)
        {
            _flushBufferLine();
        }

        _api.currentColor = newColors;
        _api.attributes = attributes;
    }
    else if (textAttributes.BackgroundIsDefault() && bg != _api.s->misc->backgroundColor)
    {
        _api.s.write()->misc.write()->backgroundColor = bg;
        _p.s.write()->misc.write()->backgroundColor = bg;
    }

    return S_OK;
}
CATCH_RETURN()

#pragma endregion

void AtlasEngine::_handleSettingsUpdate()
{
    const auto targetChanged = _p.s->target != _api.s->target;
    const auto fontChanged = _p.s->font != _api.s->font;
    const auto cellCountChanged = _p.s->cellCount != _api.s->cellCount;

    _p.s = _api.s;

    if (targetChanged)
    {
        _b.reset();
    }
    if (fontChanged)
    {
        _recreateFontDependentResources();
    }
    if (cellCountChanged)
    {
        _recreateCellCountDependentResources();
    }

    _api.invalidatedRows = invalidatedRowsAll;

    u16 y = 0;
    for (const auto r : _p.rows)
    {
        r->clear(y, _p.s->font->cellSize.y);
        ++y;
    }
}

void AtlasEngine::_recreateFontDependentResources()
{
    _api.replacementCharacterFontFace.reset();
    _api.replacementCharacterGlyphIndex = 0;
    _api.replacementCharacterLookedUp = false;

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
        // If they're NAN they haven't been set by the user and must be filled by us.
        // When we call SetFontAxisValues() we basically override (disable) DirectWrite's internal font axes,
        // and if either of the 3 aren't set we'd make it impossible for the user to see bold/italic text.
        const auto& standardAxes = _p.s->font->fontAxisValues;
        auto fontAxisValues = _p.s->font->fontAxisValues;

        for (size_t i = 0; i < 4; ++i)
        {
            const auto bold = (i & static_cast<size_t>(FontRelevantAttributes::Bold)) != 0;
            const auto italic = (i & static_cast<size_t>(FontRelevantAttributes::Italic)) != 0;
            // The wght axis defaults to the font weight.
            fontAxisValues[0].value = bold ? DWRITE_FONT_WEIGHT_BOLD : (isnan(standardAxes[0].value) ? static_cast<f32>(_p.s->font->fontWeight) : standardAxes[0].value);
            // The ital axis defaults to 1 if this is italic and 0 otherwise.
            fontAxisValues[1].value = italic ? 1.0f : (isnan(standardAxes[1].value) ? 0.0f : standardAxes[1].value);
            // The slnt axis defaults to -12 if this is italic and 0 otherwise.
            fontAxisValues[2].value = italic ? -12.0f : (isnan(standardAxes[2].value) ? 0.0f : standardAxes[2].value);
            _api.textFormatAxes[i] = { fontAxisValues.data(), fontAxisValues.size() };
        }
    }
}

void AtlasEngine::_recreateCellCountDependentResources()
{
    // Let's guess that every cell consists of a surrogate pair.
    const auto projectedTextSize = static_cast<size_t>(_p.s->cellCount.x) * 2;
    // IDWriteTextAnalyzer::GetGlyphs says:
    //   The recommended estimate for the per-glyph output buffers is (3 * textLength / 2 + 16).
    const auto projectedGlyphSize = 3 * projectedTextSize / 2 + 16;

    _api.bufferLine = std::vector<wchar_t>{};
    _api.bufferLine.reserve(projectedTextSize);
    _api.bufferLineColumn.reserve(projectedTextSize + 1);
    _api.colorsForeground = Buffer<u32>(_p.s->cellCount.x);

    _api.analysisResults = std::vector<TextAnalysisSinkResult>{};
    _api.clusterMap = Buffer<u16>{ projectedTextSize };
    _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ projectedTextSize };
    _api.glyphIndices = Buffer<u16>{ projectedGlyphSize };
    _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ projectedGlyphSize };
    _api.glyphAdvances = Buffer<f32>{ projectedGlyphSize };
    _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ projectedGlyphSize };

    _p.unorderedRows = Buffer<ShapedRow>(_p.s->cellCount.y);
    _p.rowsScratch = Buffer<ShapedRow*>(_p.s->cellCount.y);
    _p.rows = Buffer<ShapedRow*>(_p.s->cellCount.y);
    _p.backgroundBitmap = Buffer<u32>(static_cast<size_t>(_p.s->cellCount.x) * _p.s->cellCount.y);

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

    wil::com_ptr<IDWriteFontFace> mappedFontFace;

#pragma warning(suppress : 26494) // Variable 'mappedEnd' is uninitialized. Always initialize an object (type.5).
    for (u32 idx = 0, mappedEnd; idx < _api.bufferLine.size(); idx = mappedEnd)
    {
        f32 scale = 1;
        u32 mappedLength = 0;
        _mapCharacters(_api.bufferLine.data() + idx, gsl::narrow_cast<u32>(_api.bufferLine.size()) - idx, &mappedLength, &scale, mappedFontFace.put());
        mappedEnd = idx + mappedLength;

        if (!mappedFontFace)
        {
            _mapReplacementCharacter(idx, mappedEnd, row);
            continue;
        }

        const auto initialIndicesCount = row.glyphIndices.size();

        if (mappedLength > _api.glyphIndices.size())
        {
            auto size = _api.glyphIndices.size();
            size = size + (size >> 1);
            size = std::max<size_t>(size, mappedLength);
            Expects(size > _api.glyphIndices.size());
            _api.glyphIndices = Buffer<u16>{ size };
            _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ size };
        }

        // We can reuse idx here, as it'll be reset to "idx = mappedEnd" in the outer loop anyways.
        for (u32 complexityLength = 0; idx < mappedEnd; idx += complexityLength)
        {
            BOOL isTextSimple;
            THROW_IF_FAILED(_p.textAnalyzer->GetTextComplexity(_api.bufferLine.data() + idx, mappedEnd - idx, mappedFontFace.get(), &isTextSimple, &complexityLength, _api.glyphIndices.data()));

#pragma warning(suppress : 4127)
            if (isTextSimple)
            {
                for (size_t i = 0; i < complexityLength; ++i)
                {
                    const auto col1 = _api.bufferLineColumn[idx + i + 0];
                    const auto fg = _api.colorsForeground[col1];
                    const auto col2 = _api.bufferLineColumn[idx + i + 1];
                    const auto glyphAdvance = (col2 - col1) * _p.s->font->cellSize.x;
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

        const auto indicesCount = row.glyphIndices.size();
        if (indicesCount > initialIndicesCount)
        {
            row.mappings.emplace_back(std::move(mappedFontFace), _p.s->font->fontSize * scale, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(indicesCount));
        }
    }
}

void AtlasEngine::_mapCharacters(const wchar_t* text, const u32 textLength, u32* mappedLength, float* scale, IDWriteFontFace** mappedFontFace) const
{
    TextAnalysisSource analysisSource{ text, textLength };
    const auto& textFormatAxis = _api.textFormatAxes[static_cast<size_t>(_api.attributes)];

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
            /* scale              */ scale,
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
            /* scale              */ scale));

        if (font)
        {
            THROW_IF_FAILED(font->CreateFontFace(mappedFontFace));
        }
    }
}

void AtlasEngine::_mapComplex(IDWriteFontFace* mappedFontFace, u32 idx, u32 length, ShapedRow& row)
{
    _api.analysisResults.clear();

    TextAnalysisSource analysisSource{ _api.bufferLine.data(), gsl::narrow<UINT32>(_api.bufferLine.size()) };
    TextAnalysisSink analysisSink{ _api.analysisResults };
    THROW_IF_FAILED(_p.textAnalyzer->AnalyzeScript(&analysisSource, idx, length, &analysisSink));

    for (const auto& a : _api.analysisResults)
    {
        const DWRITE_SCRIPT_ANALYSIS scriptAnalysis{ a.script, static_cast<DWRITE_SCRIPT_SHAPES>(a.shapes) };
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
                /* isRightToLeft       */ a.bidiLevel & 1,
                /* scriptAnalysis      */ &scriptAnalysis,
                /* localeName          */ nullptr,
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
            /* isRightToLeft       */ a.bidiLevel & 1,
            /* scriptAnalysis      */ &scriptAnalysis,
            /* localeName          */ nullptr,
            /* features            */ &features,
            /* featureRangeLengths */ &featureRangeLengths,
            /* featureRanges       */ featureRanges,
            /* glyphAdvances       */ _api.glyphAdvances.data(),
            /* glyphOffsets        */ _api.glyphOffsets.data()));

        _api.clusterMap[a.textLength] = gsl::narrow_cast<u16>(actualGlyphCount);

        auto prevCluster = _api.clusterMap[0];
        size_t beg = 0;

        for (size_t i = 1; i <= a.textLength; ++i)
        {
            const auto nextCluster = _api.clusterMap[i];
            if (prevCluster == nextCluster)
            {
                continue;
            }

            const auto col1 = _api.bufferLineColumn[a.textPosition + beg];
            const auto col2 = _api.bufferLineColumn[a.textPosition + i];
            const auto fg = _api.colorsForeground[col1];

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
    // TODO soft fonts
#if 0
    if (!_p.s->font->softFontPattern.empty())
    {
        for (u32 i = from; i < to; ++i)
        {
            const auto ch = _api.bufferLine[from];
            if (ch >= 0xEF20 && ch < 0xEF80)
            {
            }
        }
        
        const auto initialIndicesCount = row.glyphIndices.size();
        row.mappings.emplace_back(_api.replacementCharacterFontFace, _p.s->font->fontSize, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(row.glyphIndices.size()), FontRendition::SoftFont);
    }
#endif

    if (!_api.replacementCharacterLookedUp)
    {
        bool succeeded = false;

        u32 mappedLength = 0;
        f32 scale = 1.0f;
        _mapCharacters(L"\uFFFD", 1, &mappedLength, &scale, _api.replacementCharacterFontFace.put());

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

    if (_api.replacementCharacterFontFace)
    {
        const auto initialIndicesCount = row.glyphIndices.size();
        const auto col0 = _api.bufferLineColumn[from];
        const auto col1 = _api.bufferLineColumn[to];
        const auto cols = gsl::narrow_cast<size_t>(col1 - col0);
        row.glyphIndices.insert(row.glyphIndices.end(), cols, _api.replacementCharacterGlyphIndex);
        row.glyphAdvances.insert(row.glyphAdvances.end(), cols, _p.s->font->cellSize.x);
        row.glyphOffsets.insert(row.glyphOffsets.end(), cols, DWRITE_GLYPH_OFFSET{});
        row.colors.insert(row.colors.end(), _api.colorsForeground.begin() + col0, _api.colorsForeground.begin() + col1);
        row.mappings.emplace_back(_api.replacementCharacterFontFace, _p.s->font->fontSize, gsl::narrow_cast<u32>(initialIndicesCount), gsl::narrow_cast<u32>(row.glyphIndices.size()));
    }
}
