// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BackendD2D.h"

#if ATLAS_DEBUG_SHOW_DIRTY
#include "colorbrewer.h"
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
#include "wic.h"
#endif

TIL_FAST_MATH_BEGIN

// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

BackendD2D::BackendD2D(wil::com_ptr<ID3D11Device2> device, wil::com_ptr<ID3D11DeviceContext2> deviceContext) noexcept :
    _device{ std::move(device) },
    _deviceContext{ std::move(deviceContext) }
{
}

void BackendD2D::Render(RenderingPayload& p)
{
    if (_generation != p.s.generation())
    {
        _handleSettingsUpdate(p);
    }

    _renderTarget->BeginDraw();
#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_DUMP_RENDER_TARGET
    // Invalidating the render target helps with spotting Present1() bugs.
    _renderTarget->Clear();
#endif
    _drawBackground(p);
    _drawText(p);
    _drawGridlines(p);
    _drawCursor(p);
    _drawSelection(p);
#if ATLAS_DEBUG_SHOW_DIRTY
    _debugShowDirty(p);
#endif
    THROW_IF_FAILED(_renderTarget->EndDraw());

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
    _debugDumpRenderTarget(p);
#endif
    _swapChainManager.Present(p);
}

bool BackendD2D::RequiresContinuousRedraw() noexcept
{
    return false;
}

void BackendD2D::WaitUntilCanRender() noexcept
{
    _swapChainManager.WaitUntilCanRender();
}

void BackendD2D::_handleSettingsUpdate(const RenderingPayload& p)
{
    _swapChainManager.UpdateSwapChainSettings(
        p,
        _device.get(),
        [this]() {
            _renderTarget.reset();
            _renderTarget4.reset();
            _deviceContext->ClearState();
            _deviceContext->Flush();
        },
        [this]() {
            _renderTarget.reset();
            _renderTarget4.reset();
            _deviceContext->ClearState();
        });

    const auto renderTargetChanged = !_renderTarget;
    const auto fontChanged = _fontGeneration != p.s->font.generation();
    const auto cellCountChanged = _cellCount != p.s->cellCount;

    if (renderTargetChanged)
    {
        {
            const auto surface = _swapChainManager.GetBuffer().query<IDXGISurface>();

            const D2D1_RENDER_TARGET_PROPERTIES props{
                .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
                .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
                .dpiX = static_cast<f32>(p.s->font->dpi),
                .dpiY = static_cast<f32>(p.s->font->dpi),
            };
            wil::com_ptr<ID2D1RenderTarget> renderTarget;
            THROW_IF_FAILED(p.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, renderTarget.addressof()));
            _renderTarget = renderTarget.query<ID2D1DeviceContext>();
            _renderTarget4 = renderTarget.try_query<ID2D1DeviceContext4>();

            _renderTarget->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
            _renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        }
        {
            static constexpr D2D1_COLOR_F color{ 1, 1, 1, 1 };
            THROW_IF_FAILED(_renderTarget->CreateSolidColorBrush(&color, nullptr, _brush.put()));
            _brushColor = 0xffffffff;
        }
    }

    if (!_dottedStrokeStyle)
    {
        static constexpr D2D1_STROKE_STYLE_PROPERTIES props{ .dashStyle = D2D1_DASH_STYLE_CUSTOM };
        static constexpr FLOAT dashes[2]{ 1, 2 };
        THROW_IF_FAILED(p.d2dFactory->CreateStrokeStyle(&props, &dashes[0], 2, _dottedStrokeStyle.addressof()));
    }

    if (renderTargetChanged || fontChanged)
    {
        const auto dpi = static_cast<f32>(p.s->font->dpi);
        _renderTarget->SetDpi(dpi, dpi);
        _renderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(p.s->font->antialiasingMode));
    }

    if (renderTargetChanged || fontChanged || cellCountChanged)
    {
        const D2D1_BITMAP_PROPERTIES props{
            .pixelFormat = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            .dpiX = static_cast<f32>(p.s->font->dpi),
            .dpiY = static_cast<f32>(p.s->font->dpi),
        };
        const D2D1_SIZE_U size{
            p.s->cellCount.x,
            p.s->cellCount.y,
        };
        const D2D1_MATRIX_3X2_F transform{
            .m11 = static_cast<f32>(p.s->font->cellSize.x),
            .m22 = static_cast<f32>(p.s->font->cellSize.y),
        };
        THROW_IF_FAILED(_renderTarget->CreateBitmap(size, nullptr, 0, &props, _backgroundBitmap.put()));
        THROW_IF_FAILED(_renderTarget->CreateBitmapBrush(_backgroundBitmap.get(), _backgroundBrush.put()));
        _backgroundBrush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        _backgroundBrush->SetExtendModeX(D2D1_EXTEND_MODE_MIRROR);
        _backgroundBrush->SetExtendModeY(D2D1_EXTEND_MODE_MIRROR);
        _backgroundBrush->SetTransform(&transform);
        _backgroundBitmapGeneration = {};
    }

    _generation = p.s.generation();
    _fontGeneration = p.s->font.generation();
    _cellCount = p.s->cellCount;
}

void BackendD2D::_drawBackground(const RenderingPayload& p) noexcept
{
    if (_backgroundBitmapGeneration != p.backgroundBitmapGeneration)
    {
        _backgroundBitmap->CopyFromMemory(nullptr, p.backgroundBitmap.data(), gsl::narrow_cast<UINT32>(p.backgroundBitmapStride * sizeof(u32)));
        _backgroundBitmapGeneration = p.backgroundBitmapGeneration;
    }

    // If the terminal was 120x30 cells and 1200x600 pixels large, this would draw the
    // background by upscaling a 120x30 pixel bitmap to fill the entire render target.
    const D2D1_RECT_F rect{ 0, 0, static_cast<f32>(p.s->targetSize.x), static_cast<f32>(p.s->targetSize.y) };
    _renderTarget->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
    _renderTarget->FillRectangle(&rect, _backgroundBrush.get());
    _renderTarget->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
}

void BackendD2D::_drawText(RenderingPayload& p)
{
    til::CoordType dirtyTop = til::CoordTypeMax;
    til::CoordType dirtyBottom = til::CoordTypeMin;

    // It is possible to create a "_foregroundBrush" similar to how the `_backgroundBrush` is created and
    // use that as the brush for text rendering below. That way we wouldn't have to search `row->colors` for color
    // changes and could draw entire lines of text in a single call. Unfortunately Direct2D is not particularly
    // smart if you do this and chooses to draw the given text into a way too small offscreen texture first and
    // then blends it on the screen with the given bitmap brush. While this roughly doubles the performance
    // when drawing lots of colors, the extra latency drops performance by >10x when drawing fewer colors.
    // Since fewer colors are more common, I've chosen to go with regular solid-color brushes.
    u16 y = 0;
    for (const auto row : p.rows)
    {
        auto baselineX = 0.0f;
        auto baselineY = static_cast<f32>(p.s->font->cellSize.y * y + p.s->font->baseline);

        if (y >= p.invalidatedRows.x && y < p.invalidatedRows.y)
        {
            for (const auto& m : row->mappings)
            {
                if (!m.fontFace.is_proper_font())
                {
                    continue;
                }

                const DWRITE_GLYPH_RUN glyphRun{
                    .fontFace = m.fontFace.get(),
                    .fontEmSize = m.fontEmSize,
                    .glyphCount = gsl::narrow_cast<UINT32>(m.glyphsTo - m.glyphsFrom),
                    .glyphIndices = &row->glyphIndices[m.glyphsFrom],
                    .glyphAdvances = &row->glyphAdvances[m.glyphsFrom],
                    .glyphOffsets = &row->glyphOffsets[m.glyphsFrom],
                };

                D2D1_RECT_F bounds{};
                THROW_IF_FAILED(_renderTarget->GetGlyphRunWorldBounds({ 0.0f, baselineY }, &glyphRun, DWRITE_MEASURING_MODE_NATURAL, &bounds));

                if (bounds.top < bounds.bottom)
                {
                    // If you print the top half of a double height row (DECDHL), the expectation is that only
                    // the top half is visible, which requires us to keep the clip rect at the bottom of the row.
                    // (Vice versa for the bottom half of a double height row.)
                    //
                    // Since we used SetUnitMode(D2D1_UNIT_MODE_PIXELS), bounds.top/bottom is in pixels already and requires no conversion nor rounding.
                    if (row->lineRendition != LineRendition::DoubleHeightBottom)
                    {
                        row->dirtyTop = std::min(row->dirtyTop, static_cast<i32>(lrintf(bounds.top)));
                    }
                    if (row->lineRendition != LineRendition::DoubleHeightTop)
                    {
                        row->dirtyBottom = std::max(row->dirtyBottom, static_cast<i32>(lrintf(bounds.bottom)));
                    }
                }
            }

            dirtyTop = std::min(dirtyTop, row->dirtyTop);
            dirtyBottom = std::max(dirtyBottom, row->dirtyBottom);
        }

        const D2D1_RECT_F clipRect{
            0,
            static_cast<f32>(row->dirtyTop),
            static_cast<f32>(p.s->targetSize.x),
            static_cast<f32>(row->dirtyBottom),
        };
        _renderTarget->PushAxisAlignedClip(&clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

        if (row->lineRendition != LineRendition::SingleWidth)
        {
            baselineY = _drawTextPrepareLineRendition(p, baselineY, row->lineRendition);
        }

        for (const auto& m : row->mappings)
        {
            const auto colorsBegin = row->colors.begin();
            auto it = colorsBegin + m.glyphsFrom;
            const auto end = colorsBegin + m.glyphsTo;

            while (it != end)
            {
                const auto beg = it;
                const auto off = it - colorsBegin;
                const auto fg = *it;

                while (++it != end && *it == fg)
                {
                }

                const auto count = it - beg;
                const auto brush = _brushWithColor(fg);
                const DWRITE_GLYPH_RUN glyphRun{
                    .fontFace = m.fontFace.get(),
                    .fontEmSize = m.fontEmSize,
                    .glyphCount = gsl::narrow_cast<UINT32>(count),
                    .glyphIndices = &row->glyphIndices[off],
                    .glyphAdvances = &row->glyphAdvances[off],
                    .glyphOffsets = &row->glyphOffsets[off],
                };
                const D2D1_POINT_2F baselineOrigin{
                    baselineX,
                    baselineY,
                };

                if (m.fontFace.is_proper_font())
                {
                    DrawGlyphRun(_renderTarget.get(), _renderTarget4.get(), p.dwriteFactory4.get(), baselineOrigin, &glyphRun, brush);
                }

                for (UINT32 i = 0; i < glyphRun.glyphCount; ++i)
                {
                    baselineX += glyphRun.glyphAdvances[i];
                }
            }
        }

        if (row->lineRendition != LineRendition::SingleWidth)
        {
            _drawTextResetLineRendition();
        }

        _renderTarget->PopAxisAlignedClip();

        ++y;
    }

    if (dirtyTop < dirtyBottom)
    {
        p.dirtyRectInPx.top = std::min(p.dirtyRectInPx.top, dirtyTop);
        p.dirtyRectInPx.bottom = std::max(p.dirtyRectInPx.bottom, dirtyBottom);
    }
}

f32 BackendD2D::_drawTextPrepareLineRendition(const RenderingPayload& p, f32 baselineY, LineRendition lineRendition) const noexcept
{
    const auto descender = static_cast<f32>(p.s->font->cellSize.y - p.s->font->baseline);
    D2D1_MATRIX_3X2_F transform{
        .m11 = 2.0f,
        .m22 = 1.0f,
    };

    if (lineRendition >= LineRendition::DoubleHeightTop)
    {
        transform.m22 = 2.0f;
        transform.dy = -1.0f * (baselineY + descender);

        if (lineRendition == LineRendition::DoubleHeightTop)
        {
            const auto delta = static_cast<f32>(p.s->font->cellSize.y);
            baselineY += delta;
            transform.dy -= delta;
        }
    }

    _renderTarget->SetTransform(&transform);
    return baselineY;
}

void BackendD2D::_drawTextResetLineRendition() const noexcept
{
    static constexpr D2D1_MATRIX_3X2_F identity{ .m11 = 1, .m22 = 1 };
    _renderTarget->SetTransform(&identity);
}

// Returns the theoretical/design design size of the given `DWRITE_GLYPH_RUN`, relative the the given baseline origin.
// This algorithm replicates what DirectWrite does internally to provide `IDWriteTextLayout::GetMetrics`.
f32r BackendD2D::_getGlyphRunDesignBounds(const DWRITE_GLYPH_RUN& glyphRun, f32 baselineX, f32 baselineY)
{
    DWRITE_FONT_METRICS fontMetrics;
    glyphRun.fontFace->GetMetrics(&fontMetrics);

    if (glyphRun.glyphCount > _glyphMetrics.size())
    {
        // Growth factor 1.5x.
        auto size = _glyphMetrics.size();
        size = size + (size >> 1);
        size = std::max<size_t>(size, glyphRun.glyphCount);
        // Overflow check.
        Expects(size > _glyphMetrics.size());
        _glyphMetrics = Buffer<DWRITE_GLYPH_METRICS>{ size };
    }

    glyphRun.fontFace->GetDesignGlyphMetrics(glyphRun.glyphIndices, glyphRun.glyphCount, _glyphMetrics.data(), false);

    const f32 fontScale = glyphRun.fontEmSize / fontMetrics.designUnitsPerEm;
    f32r accumulatedBounds{
        baselineX,
        baselineY,
        baselineX,
        baselineY,
    };

    for (uint32_t i = 0; i < glyphRun.glyphCount; ++i)
    {
        const auto& glyphMetrics = _glyphMetrics[i];
        const auto glyphAdvance = glyphRun.glyphAdvances ? glyphRun.glyphAdvances[i] : glyphMetrics.advanceWidth * fontScale;

        const auto left = static_cast<f32>(glyphMetrics.leftSideBearing) * fontScale;
        const auto top = static_cast<f32>(glyphMetrics.topSideBearing - glyphMetrics.verticalOriginY) * fontScale;
        const auto right = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceWidth) - glyphMetrics.rightSideBearing) * fontScale;
        const auto bottom = static_cast<f32>(gsl::narrow_cast<INT32>(glyphMetrics.advanceHeight) - glyphMetrics.bottomSideBearing - glyphMetrics.verticalOriginY) * fontScale;

        if (left < right && top < bottom)
        {
            auto glyphX = baselineX;
            auto glyphY = baselineY;
            if (glyphRun.glyphOffsets)
            {
                glyphX += glyphRun.glyphOffsets[i].advanceOffset;
                glyphY -= glyphRun.glyphOffsets[i].ascenderOffset;
            }

            accumulatedBounds.left = std::min(accumulatedBounds.left, left + glyphX);
            accumulatedBounds.top = std::min(accumulatedBounds.top, top + glyphY);
            accumulatedBounds.right = std::max(accumulatedBounds.right, right + glyphX);
            accumulatedBounds.bottom = std::max(accumulatedBounds.bottom, bottom + glyphY);
        }

        baselineX += glyphAdvance;
    }

    return accumulatedBounds;
}

void BackendD2D::_drawGridlines(const RenderingPayload& p)
{
    u16 y = 0;
    for (const auto row : p.rows)
    {
        if (!row->gridLineRanges.empty())
        {
            _drawGridlineRow(p, row, y);
        }
        y++;
    }
}

void BackendD2D::_drawGridlineRow(const RenderingPayload& p, const ShapedRow* row, u16 y)
{
    const auto columnToDIP = [&](til::CoordType i) {
        return i * p.s->font->cellSize.x;
    };
    const auto rowToDIP = [&](til::CoordType i) {
        return i * p.s->font->cellSize.y;
    };

    const auto top = rowToDIP(y);
    const auto bottom = top + p.s->font->cellSize.y;

    for (const auto& r : row->gridLineRanges)
    {
        // AtlasEngine.cpp shouldn't add any gridlines if they don't do anything.
        assert(r.lines.any());

        const auto left = columnToDIP(r.from);
        const auto right = columnToDIP(r.to);
        til::rect rect;

        if (r.lines.test(GridLines::Left))
        {
            rect.top = top;
            rect.bottom = bottom;
            for (auto i = r.from; i < r.to; ++i)
            {
                rect.left = columnToDIP(i);
                rect.right = rect.left + p.s->font->thinLineWidth;
                _fillRectangle(rect, r.color);
            }
        }
        if (r.lines.test(GridLines::Top))
        {
            rect.left = left;
            rect.top = top;
            rect.right = right;
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _fillRectangle(rect, r.color);
        }
        if (r.lines.test(GridLines::Right))
        {
            rect.top = top;
            rect.bottom = bottom;
            for (auto i = r.to; i > r.from; --i)
            {
                rect.right = columnToDIP(i);
                rect.left = rect.right - p.s->font->thinLineWidth;
                _fillRectangle(rect, r.color);
            }
        }
        if (r.lines.test(GridLines::Bottom))
        {
            rect.left = left;
            rect.top = bottom - p.s->font->thinLineWidth;
            rect.right = right;
            rect.bottom = bottom;
            _fillRectangle(rect, r.color);
        }
        if (r.lines.test(GridLines::Underline))
        {
            rect.left = left;
            rect.top = top + p.s->font->underlinePos;
            rect.right = right;
            rect.bottom = rect.top + p.s->font->underlineWidth;
            _fillRectangle(rect, r.color);
        }
        if (r.lines.test(GridLines::HyperlinkUnderline))
        {
            const auto w = p.s->font->underlineWidth;
            const auto centerY = (top + p.s->font->underlinePos) + w * 0.5f;
            const auto brush = _brushWithColor(r.color);
            const D2D1_POINT_2F point0{ static_cast<f32>(left), centerY };
            const D2D1_POINT_2F point1{ static_cast<f32>(right), centerY };
            _renderTarget->DrawLine(point0, point1, brush, w, _dottedStrokeStyle.get());
        }
        if (r.lines.test(GridLines::DoubleUnderline))
        {
            rect.left = left;
            rect.top = top + p.s->font->doubleUnderlinePos.x;
            rect.right = right;
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _fillRectangle(rect, r.color);

            rect.left = left;
            rect.top = top + p.s->font->doubleUnderlinePos.y;
            rect.right = right;
            rect.bottom = rect.top + p.s->font->thinLineWidth;
            _fillRectangle(rect, r.color);
        }
        if (r.lines.test(GridLines::Strikethrough))
        {
            rect.left = left;
            rect.top = top + p.s->font->strikethroughPos;
            rect.right = right;
            rect.bottom = rect.top + p.s->font->strikethroughWidth;
            _fillRectangle(rect, r.color);
        }
    }
}

void BackendD2D::_drawCursor(const RenderingPayload& p)
{
    if (p.cursorRect.empty())
    {
        return;
    }

    // Inverted cursors could be implemented in the future using
    // ID2D1DeviceContext::DrawImage and D2D1_COMPOSITE_MODE_MASK_INVERT.

    til::rect rect{
        p.s->font->cellSize.x * p.cursorRect.left,
        p.s->font->cellSize.y * p.cursorRect.top,
        p.s->font->cellSize.x * p.cursorRect.right,
        p.s->font->cellSize.y * p.cursorRect.bottom,
    };

    switch (static_cast<CursorType>(p.s->cursor->cursorType))
    {
    case CursorType::Legacy:
        rect.top = rect.bottom - (p.s->font->cellSize.y * p.s->cursor->heightPercentage + 50) / 100;
        _fillRectangle(rect, p.s->cursor->cursorColor);
        break;
    case CursorType::VerticalBar:
        rect.right = rect.left + p.s->font->thinLineWidth;
        _fillRectangle(rect, p.s->cursor->cursorColor);
        break;
    case CursorType::Underscore:
        rect.top += p.s->font->underlinePos;
        rect.bottom = rect.top + p.s->font->underlineWidth;
        _fillRectangle(rect, p.s->cursor->cursorColor);
        break;
    case CursorType::EmptyBox:
    {
        const auto brush = _brushWithColor(p.s->cursor->cursorColor);
        const auto w = p.s->font->thinLineWidth;
        const auto wh = w / 2.0f;
        const D2D1_RECT_F rectF{
            rect.left + wh,
            rect.top + wh,
            rect.right - wh,
            rect.bottom - wh,
        };
        _renderTarget->DrawRectangle(&rectF, brush, w, nullptr);
        break;
    }
    case CursorType::FullBox:
        _fillRectangle(rect, p.s->cursor->cursorColor);
        break;
    case CursorType::DoubleUnderscore:
    {
        auto rect2 = rect;
        rect2.top = rect.top + p.s->font->doubleUnderlinePos.x;
        rect2.bottom = rect2.top + p.s->font->thinLineWidth;
        _fillRectangle(rect2, p.s->cursor->cursorColor);
        rect.top = rect.top + p.s->font->doubleUnderlinePos.y;
        rect.bottom = rect.top + p.s->font->thinLineWidth;
        _fillRectangle(rect, p.s->cursor->cursorColor);
        break;
    }
    default:
        break;
    }
}

void BackendD2D::_drawSelection(const RenderingPayload& p)
{
    u16 y = 0;
    for (const auto& row : p.rows)
    {
        if (row->selectionTo > row->selectionFrom)
        {
            const D2D1_RECT_F rect{
                static_cast<f32>(p.s->font->cellSize.x * row->selectionFrom),
                static_cast<f32>(p.s->font->cellSize.y * y),
                static_cast<f32>(p.s->font->cellSize.x * row->selectionTo),
                static_cast<f32>(p.s->font->cellSize.y * (y + 1)),
            };
            _fillRectangle(rect, p.s->misc->selectionColor);
        }

        y++;
    }
}

#if ATLAS_DEBUG_SHOW_DIRTY
void BackendD2D::_debugShowDirty(const RenderingPayload& p)
{
    _presentRects[_presentRectsPos] = p.dirtyRectInPx;
    _presentRectsPos = (_presentRectsPos + 1) % std::size(_presentRects);

    for (size_t i = 0; i < std::size(_presentRects); ++i)
    {
        if (const auto& rect = _presentRects[i])
        {
            const D2D1_RECT_F rectF{
                static_cast<f32>(rect.left),
                static_cast<f32>(rect.top),
                static_cast<f32>(rect.right),
                static_cast<f32>(rect.bottom),
            };
            const auto color = colorbrewer::pastel1[i] | 0x1f000000;
            _fillRectangle(rectF, color);
        }
    }
}
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
void BackendD2D::_debugDumpRenderTarget(const RenderingPayload& p)
{
    if (_dumpRenderTargetCounter == 0)
    {
        ExpandEnvironmentStringsW(ATLAS_DEBUG_DUMP_RENDER_TARGET_PATH, &_dumpRenderTargetBasePath[0], gsl::narrow_cast<DWORD>(std::size(_dumpRenderTargetBasePath)));
        std::filesystem::create_directories(_dumpRenderTargetBasePath);
    }

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\%u_%08zu.png", &_dumpRenderTargetBasePath[0], GetCurrentProcessId(), _dumpRenderTargetCounter);
    SaveTextureToPNG(_deviceContext.get(), _swapChainManager.GetBuffer().get(), p.s->font->dpi, &path[0]);
    _dumpRenderTargetCounter++;
}
#endif

ID2D1Brush* BackendD2D::_brushWithColor(u32 color)
{
    if (_brushColor != color)
    {
        const auto d2dColor = colorFromU32(color);
        THROW_IF_FAILED(_renderTarget->CreateSolidColorBrush(&d2dColor, nullptr, _brush.put()));
        _brushColor = color;
    }
    return _brush.get();
}

void BackendD2D::_fillRectangle(const til::rect& rect, u32 color)
{
    const D2D1_RECT_F rectF{
        static_cast<f32>(rect.left),
        static_cast<f32>(rect.top),
        static_cast<f32>(rect.right),
        static_cast<f32>(rect.bottom),
    };
    _fillRectangle(rectF, color);
}

void BackendD2D::_fillRectangle(const D2D1_RECT_F& rect, u32 color)
{
    const auto brush = _brushWithColor(color);
    _renderTarget->FillRectangle(&rect, brush);
}

TIL_FAST_MATH_END
