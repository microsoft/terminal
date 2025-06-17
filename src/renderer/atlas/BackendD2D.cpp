// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BackendD2D.h"

#include <til/unicode.h>

#if ATLAS_DEBUG_SHOW_DIRTY
#include <til/colorbrewer.h>
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

void BackendD2D::ReleaseResources() noexcept
{
    _renderTarget.reset();
    _renderTarget4.reset();
    // Ensure _handleSettingsUpdate() is called so that _renderTarget gets recreated.
    _generation = {};
}

void BackendD2D::Render(RenderingPayload& p)
{
    if (_generation != p.s.generation())
    {
        _handleSettingsUpdate(p);
    }

    _renderTarget->BeginDraw();
    try
    {
#if ATLAS_DEBUG_SHOW_DIRTY || ATLAS_DEBUG_DUMP_RENDER_TARGET
        // Invalidating the render target helps with spotting Present1() bugs.
        _renderTarget->Clear();
#endif
        _drawBackground(p);
        _drawCursorPart1(p);
        _drawText(p);
        _drawCursorPart2(p);
#if ATLAS_DEBUG_SHOW_DIRTY
        _debugShowDirty(p);
#endif
    }
    catch (...)
    {
        // In case an exception is thrown for some reason between BeginDraw() and EndDraw()
        // we still technically need to call EndDraw() before releasing any resources.
        LOG_IF_FAILED(_renderTarget->EndDraw());
        throw;
    }
    THROW_IF_FAILED(_renderTarget->EndDraw());

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
    _debugDumpRenderTarget(p);
#endif
}

bool BackendD2D::RequiresContinuousRedraw() noexcept
{
    return false;
}

void BackendD2D::_handleSettingsUpdate(const RenderingPayload& p)
{
    const auto renderTargetChanged = !_renderTarget;
    const auto fontChanged = _fontGeneration != p.s->font.generation();
    const auto cursorChanged = _cursorGeneration != p.s->cursor.generation();
    const auto backgroundColorChanged = _miscGeneration != p.s->misc.generation();
    const auto cellCountChanged = _viewportCellCount != p.s->viewportCellCount;

    if (renderTargetChanged)
    {
        {
            wil::com_ptr<ID3D11Texture2D> buffer;
            THROW_IF_FAILED(p.swapChain.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(buffer.addressof())));
            const auto surface = buffer.query<IDXGISurface>();

            const D2D1_RENDER_TARGET_PROPERTIES props{
                .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
                .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
                .dpiX = static_cast<f32>(p.s->font->dpi),
                .dpiY = static_cast<f32>(p.s->font->dpi),
            };
            // ID2D1RenderTarget and ID2D1DeviceContext are the same and I'm tired of pretending they're not.
            THROW_IF_FAILED(p.d2dFactory->CreateDxgiSurfaceRenderTarget(surface.get(), &props, reinterpret_cast<ID2D1RenderTarget**>(_renderTarget.put())));

            _renderTarget->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

            _renderTarget.try_query_to(_renderTarget4.put());
            if (_renderTarget4)
            {
                THROW_IF_FAILED(_renderTarget4->CreateSpriteBatch(_builtinGlyphBatch.put()));
            }
        }
        {
            static constexpr D2D1_COLOR_F color{};
            THROW_IF_FAILED(_renderTarget->CreateSolidColorBrush(&color, nullptr, _emojiBrush.put()));
            THROW_IF_FAILED(_renderTarget->CreateSolidColorBrush(&color, nullptr, _brush.put()));
            _brushColor = 0;
        }
    }

    if (renderTargetChanged || fontChanged)
    {
        const auto dpi = static_cast<f32>(p.s->font->dpi);
        _renderTarget->SetDpi(dpi, dpi);
        _renderTarget->SetTextAntialiasMode(static_cast<D2D1_TEXT_ANTIALIAS_MODE>(p.s->font->antialiasingMode));

        _builtinGlyphsRenderTarget.reset();
        _builtinGlyphsBitmap.reset();
        _builtinGlyphsRenderTargetActive = false;
    }

    if (renderTargetChanged || fontChanged || cellCountChanged || backgroundColorChanged)
    {
        const D2D1_BITMAP_PROPERTIES props{
            .pixelFormat = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
            .dpiX = static_cast<f32>(p.s->font->dpi),
            .dpiY = static_cast<f32>(p.s->font->dpi),
        };
        const D2D1_SIZE_U size{
            p.s->viewportCellCount.x + 2u,
            p.s->viewportCellCount.y + 2u,
        };
        const D2D1_MATRIX_3X2_F transform{
            .m11 = static_cast<f32>(p.s->font->cellSize.x),
            .m22 = static_cast<f32>(p.s->font->cellSize.y),
            /* Brushes are transformed relative to the render target, not the rect into which they are painted. */
            .dx = -static_cast<f32>(p.s->font->cellSize.x),
            .dy = -static_cast<f32>(p.s->font->cellSize.y),
        };

        /*
         We're allocating a bitmap that is one pixel wider on every side than the viewport so that we can fill in the gutter
         with the background color. D2D doesn't have an equivalent to D3D11_TEXTURE_ADDRESS_BORDER, which we use in the D3D
         backend to ensure the colors don't bleed off the edges.

         XXXXXXXXXXXXXXXX <- background color
         X+------------+X
         X| viewport   |X
         X|            |X
         X|            |X
         X+------------+X
         XXXXXXXXXXXXXXXX

         The translation in `transform` ensures that we render it off the top left of the render target.
        */
        auto backgroundFill = std::make_unique_for_overwrite<u32[]>(static_cast<size_t>(size.width) * size.height);
        std::fill_n(backgroundFill.get(), size.width * size.height, u32ColorPremultiply(p.s->misc->backgroundColor));

        THROW_IF_FAILED(_renderTarget->CreateBitmap(size, backgroundFill.get(), size.width * sizeof(u32), &props, _backgroundBitmap.put()));
        THROW_IF_FAILED(_renderTarget->CreateBitmapBrush(_backgroundBitmap.get(), _backgroundBrush.put()));
        _backgroundBrush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        _backgroundBrush->SetExtendModeX(D2D1_EXTEND_MODE_CLAMP);
        _backgroundBrush->SetExtendModeY(D2D1_EXTEND_MODE_CLAMP);
        _backgroundBrush->SetTransform(&transform);
        _backgroundBitmapGeneration = {};
    }

    if (fontChanged || cursorChanged)
    {
        _cursorBitmap.reset();
        _cursorBitmapSize = {};
    }

    _generation = p.s.generation();
    _fontGeneration = p.s->font.generation();
    _cursorGeneration = p.s->cursor.generation();
    _miscGeneration = p.s->misc.generation();
    _viewportCellCount = p.s->viewportCellCount;
}

void BackendD2D::_drawBackground(const RenderingPayload& p)
{
    if (_backgroundBitmapGeneration != p.colorBitmapGenerations[0])
    {
        const D2D1_RECT_U rect{
            1u,
            1u,
            1u + p.s->viewportCellCount.x,
            1u + p.s->viewportCellCount.y,
        };
        THROW_IF_FAILED(_backgroundBitmap->CopyFromMemory(&rect, p.backgroundBitmap.data(), gsl::narrow_cast<UINT32>(p.colorBitmapRowStride * sizeof(u32))));
        _backgroundBitmapGeneration = p.colorBitmapGenerations[0];
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

        if (row->lineRendition != LineRendition::SingleWidth)
        {
            baselineY = _drawTextPrepareLineRendition(p, row, baselineY);
        }

        for (const auto& m : row->mappings)
        {
            if (!m.fontFace)
            {
                baselineX = _drawBuiltinGlyphs(p, row, m, baselineY, baselineX);
                continue;
            }

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
                    .fontEmSize = p.s->font->fontSize,
                    .glyphCount = gsl::narrow_cast<UINT32>(count),
                    .glyphIndices = &row->glyphIndices[off],
                    .glyphAdvances = &row->glyphAdvances[off],
                    .glyphOffsets = &row->glyphOffsets[off],
                };
                const D2D1_POINT_2F baselineOrigin{
                    baselineX,
                    baselineY,
                };

                D2D1_RECT_F bounds = GlyphRunEmptyBounds;
                wil::com_ptr<IDWriteColorGlyphRunEnumerator1> enumerator;

                if (p.s->font->colorGlyphs)
                {
                    enumerator = TranslateColorGlyphRun(p.dwriteFactory4.get(), baselineOrigin, &glyphRun);
                }

                if (enumerator)
                {
                    while (ColorGlyphRunMoveNext(enumerator.get()))
                    {
                        const auto colorGlyphRun = ColorGlyphRunGetCurrentRun(enumerator.get());
                        ColorGlyphRunDraw(_renderTarget4.get(), _emojiBrush.get(), brush, colorGlyphRun);
                        ColorGlyphRunAccumulateBounds(_renderTarget.get(), colorGlyphRun, bounds);
                    }
                }
                else
                {
                    _renderTarget->DrawGlyphRun(baselineOrigin, &glyphRun, brush, DWRITE_MEASURING_MODE_NATURAL);
                    GlyphRunAccumulateBounds(_renderTarget.get(), baselineOrigin, &glyphRun, bounds);
                }

                if (bounds.top < bounds.bottom)
                {
                    // Since we used SetUnitMode(D2D1_UNIT_MODE_PIXELS), bounds.top/bottom is in pixels already and requires no conversion/rounding.
                    if (row->lineRendition != LineRendition::DoubleHeightTop)
                    {
                        row->dirtyBottom = std::max(row->dirtyBottom, static_cast<i32>(lrintf(bounds.bottom)));
                    }
                    if (row->lineRendition != LineRendition::DoubleHeightBottom)
                    {
                        row->dirtyTop = std::min(row->dirtyTop, static_cast<i32>(lrintf(bounds.top)));
                    }
                }

                for (UINT32 i = 0; i < glyphRun.glyphCount; ++i)
                {
                    baselineX += glyphRun.glyphAdvances[i];
                }
            }
        }

        _flushBuiltinGlyphs();

        if (!row->gridLineRanges.empty())
        {
            _drawGridlineRow(p, row, y);
        }

        if (row->lineRendition != LineRendition::SingleWidth)
        {
            _drawTextResetLineRendition(row);
        }

        if (row->bitmap.revision != 0)
        {
            _drawBitmap(p, row, y);
        }

        if (p.invalidatedRows.contains(y))
        {
            dirtyTop = std::min(dirtyTop, row->dirtyTop);
            dirtyBottom = std::max(dirtyBottom, row->dirtyBottom);
        }

        ++y;
    }

    if (dirtyTop < dirtyBottom)
    {
        p.dirtyRectInPx.top = std::min(p.dirtyRectInPx.top, dirtyTop);
        p.dirtyRectInPx.bottom = std::max(p.dirtyRectInPx.bottom, dirtyBottom);
    }
}

f32 BackendD2D::_drawBuiltinGlyphs(const RenderingPayload& p, const ShapedRow* row, const FontMapping& m, f32 baselineY, f32 baselineX)
{
    const f32 cellTop = baselineY - p.s->font->baseline;
    const f32 cellBottom = cellTop + p.s->font->cellSize.y;
    const f32 cellWidth = p.s->font->cellSize.x;

    _prepareBuiltinGlyphRenderTarget(p);

    for (size_t i = m.glyphsFrom; i < m.glyphsTo; ++i)
    {
        // This code runs when fontFace == nullptr. This is only the case for builtin glyphs which then use the glyphIndices
        // to store UTF16 code points. In other words, this doesn't accidentally corrupt any actual glyph indices.
        u32 ch = row->glyphIndices[i];
        if (til::is_leading_surrogate(ch))
        {
            i += 1;
            ch = til::combine_surrogates(ch, row->glyphIndices[i]);
        }

        // If we don't have support for ID2D1SpriteBatch we don't support builtin glyphs.
        // But we do still need to account for the glyphAdvances, which is why we can't just skip everything.
        // It's very unlikely for a target device to not support ID2D1SpriteBatch as it's very old at this point.
        if (_builtinGlyphBatch)
        {
            if (const auto off = BuiltinGlyphs::GetBitmapCellIndex(ch); off >= 0)
            {
                const D2D1_RECT_F dst{ baselineX, cellTop, baselineX + cellWidth, cellBottom };
                const auto src = _prepareBuiltinGlyph(p, ch, off);
                const auto color = colorFromU32(row->colors[i]);
                THROW_IF_FAILED(_builtinGlyphBatch->AddSprites(1, &dst, &src, &color, nullptr, sizeof(D2D1_RECT_F), sizeof(D2D1_RECT_U), sizeof(D2D1_COLOR_F), sizeof(D2D1_MATRIX_3X2_F)));
            }
        }

        baselineX += row->glyphAdvances[i];
    }

    return baselineX;
}

void BackendD2D::_prepareBuiltinGlyphRenderTarget(const RenderingPayload& p)
{
    // If we don't have support for ID2D1SpriteBatch none of the related members will be initialized or used.
    // We can just early-return in that case.
    if (!_builtinGlyphBatch)
    {
        return;
    }

    // If the render target is already created, all of the below has already been done in a previous frame.
    // Once the relevant settings change for some reason (primarily the font->cellSize), then _handleSettingsUpdate()
    // will reset the render target which will cause us to skip this condition and re-initialize it below.
    if (_builtinGlyphsRenderTarget)
    {
        return;
    }

    const auto cellWidth = static_cast<u32>(p.s->font->cellSize.x);
    const auto cellHeight = static_cast<u32>(p.s->font->cellSize.y);
    const auto cellArea = cellWidth * cellHeight;
    const auto area = cellArea * BuiltinGlyphs::TotalCharCount;

    // This block of code calculates the size of a power-of-2 texture that has an area larger than the given `area`.
    // For instance, for an area of 985x1946 = 1916810 it would result in a u/v of 2048x1024 (area = 2097152).
    // We throw the "v" in this case away, because we don't really need power-of-2 textures here,
    // but you can find the complete code over in BackendD3D. If someone deleted it in the meantime:
    //   const auto index = bitness_of_area_minus_1 - std::countl_zero(area - 1); // aka: _BitScanReverse
    //   const auto u = 1u << ((index + 2) / 2);
    //   const auto v = 1u << ((index + 1) / 2);
    unsigned long index;
    _BitScanReverse(&index, area - 1);
    const auto potWidth = 1u << ((index + 2) / 2);

    const auto cellCountU = potWidth / cellWidth;
    const auto cellCountV = (BuiltinGlyphs::TotalCharCount + cellCountU - 1) / cellCountU;
    const auto u = cellCountU * cellWidth;
    const auto v = cellCountV * cellHeight;

    const D2D1_SIZE_F sizeF{ static_cast<f32>(u), static_cast<f32>(v) };
    const D2D1_SIZE_U sizeU{ gsl::narrow_cast<UINT32>(u), gsl::narrow_cast<UINT32>(v) };
    static constexpr D2D1_PIXEL_FORMAT format{ DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
    wil::com_ptr<ID2D1BitmapRenderTarget> target;
    THROW_IF_FAILED(_renderTarget->CreateCompatibleRenderTarget(&sizeF, &sizeU, &format, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, target.addressof()));

    THROW_IF_FAILED(target->GetBitmap(_builtinGlyphsBitmap.put()));
    _builtinGlyphsRenderTarget = target.query<ID2D1DeviceContext>();
    _builtinGlyphsBitmapCellCountU = cellCountU;
    memset(&_builtinGlyphsReady[0], 0, sizeof(_builtinGlyphsReady));

    _builtinGlyphsRenderTarget->BeginDraw();
    _builtinGlyphsRenderTargetActive = true;

    // The initial contents of the bitmap are undefined.
    // -> We need to define them. :)
    _builtinGlyphsRenderTarget->Clear();
}

D2D1_RECT_U BackendD2D::_prepareBuiltinGlyph(const RenderingPayload& p, char32_t ch, u32 off)
{
    const u32 w = p.s->font->cellSize.x;
    const u32 h = p.s->font->cellSize.y;
    const u32 l = (off % _builtinGlyphsBitmapCellCountU) * w;
    const u32 t = (off / _builtinGlyphsBitmapCellCountU) * h;
    D2D1_RECT_U rectU{ l, t, l + w, t + h };

    // Check if we previously cached this glyph already.
    if (_builtinGlyphsReady[off])
    {
        return rectU;
    }

    static constexpr D2D1_COLOR_F shadeColorMap[] = {
        { 1, 1, 1, 0.25f }, // Shape_Filled025
        { 1, 1, 1, 0.50f }, // Shape_Filled050
        { 1, 1, 1, 0.75f }, // Shape_Filled075
        { 1, 1, 1, 1.00f }, // Shape_Filled100
    };

    if (!_builtinGlyphsRenderTargetActive)
    {
        _builtinGlyphsRenderTarget->BeginDraw();
        _builtinGlyphsRenderTargetActive = true;
    }

    const auto brush = _brushWithColor(0xffffffff);
    const D2D1_RECT_F rectF{
        static_cast<f32>(rectU.left),
        static_cast<f32>(rectU.top),
        static_cast<f32>(rectU.right),
        static_cast<f32>(rectU.bottom),
    };
    BuiltinGlyphs::DrawBuiltinGlyph(p.d2dFactory.get(), _builtinGlyphsRenderTarget.get(), brush, shadeColorMap, rectF, ch);

    _builtinGlyphsReady[off] = true;
    return rectU;
}

void BackendD2D::_flushBuiltinGlyphs()
{
    // If we don't have support for ID2D1SpriteBatch none of the related members will be initialized or used.
    // We can just early-return in that case.
    if (!_builtinGlyphBatch)
    {
        return;
    }

    if (_builtinGlyphsRenderTargetActive)
    {
        THROW_IF_FAILED(_builtinGlyphsRenderTarget->EndDraw());
        _builtinGlyphsRenderTargetActive = false;
    }

    if (const auto count = _builtinGlyphBatch->GetSpriteCount(); count > 0)
    {
        _renderTarget4->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        _renderTarget4->DrawSpriteBatch(_builtinGlyphBatch.get(), 0, count, _builtinGlyphsBitmap.get(), D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_SPRITE_OPTIONS_NONE);
        _renderTarget4->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        _builtinGlyphBatch->Clear();
    }
}

f32 BackendD2D::_drawTextPrepareLineRendition(const RenderingPayload& p, const ShapedRow* row, f32 baselineY) const noexcept
{
    const auto lineRendition = row->lineRendition;
    D2D1_MATRIX_3X2_F transform{
        .m11 = 2.0f,
        .m22 = 1.0f,
    };

    if (lineRendition >= LineRendition::DoubleHeightTop)
    {
        D2D1_RECT_F clipRect{ 0, 0, static_cast<f32>(p.s->targetSize.x), static_cast<f32>(p.s->targetSize.y) };

        transform.m22 = 2.0f;
        transform.dy = -1.0f * (baselineY + p.s->font->descender);

        // If you print the top half of a double height row (DECDHL), the expectation is that only
        // the top half is visible, which requires us to keep the clip rect at the bottom of the row.
        // (Vice versa for the bottom half of a double height row.)
        if (lineRendition == LineRendition::DoubleHeightTop)
        {
            const auto delta = static_cast<f32>(p.s->font->cellSize.y);
            baselineY += delta;
            transform.dy -= delta;
            clipRect.bottom = static_cast<f32>(row->dirtyBottom);
        }
        else
        {
            clipRect.top = static_cast<f32>(row->dirtyTop);
        }

        _renderTarget->PushAxisAlignedClip(&clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
    }

    _renderTarget->SetTransform(&transform);
    return baselineY;
}

void BackendD2D::_drawTextResetLineRendition(const ShapedRow* row) const noexcept
{
    static constexpr D2D1_MATRIX_3X2_F identity{ .m11 = 1, .m22 = 1 };
    _renderTarget->SetTransform(&identity);

    if (row->lineRendition >= LineRendition::DoubleHeightTop)
    {
        _renderTarget->PopAxisAlignedClip();
    }
}

// Returns the theoretical/design design size of the given `DWRITE_GLYPH_RUN`, relative the given baseline origin.
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

    THROW_IF_FAILED(glyphRun.fontFace->GetDesignGlyphMetrics(glyphRun.glyphIndices, glyphRun.glyphCount, _glyphMetrics.data(), false));

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

void BackendD2D::_drawGridlineRow(const RenderingPayload& p, const ShapedRow* row, u16 y)
{
    const auto cellWidth = static_cast<f32>(p.s->font->cellSize.x);
    const auto cellHeight = static_cast<f32>(p.s->font->cellSize.y);
    const auto rowTop = cellHeight * y;
    const auto rowBottom = rowTop + cellHeight;
    const auto cellCenter = row->lineRendition == LineRendition::DoubleHeightTop ? rowBottom : rowTop;
    const auto scaleHorizontal = row->lineRendition != LineRendition::SingleWidth ? 0.5f : 1.0f;
    const auto scaledCellWidth = cellWidth * scaleHorizontal;

    const auto appendVerticalLines = [&](const GridLineRange& r, FontDecorationPosition pos) {
        const auto from = r.from * scaledCellWidth;
        const auto to = r.to * scaledCellWidth;
        auto x = from + pos.position;

        D2D1_POINT_2F point0{ 0, cellCenter };
        D2D1_POINT_2F point1{ 0, cellCenter + cellHeight };
        const auto brush = _brushWithColor(r.gridlineColor);
        const f32 w = pos.height;
        const f32 hw = w * 0.5f;

        for (; x < to; x += cellWidth)
        {
            const auto centerX = x + hw;
            point0.x = centerX;
            point1.x = centerX;
            _renderTarget->DrawLine(point0, point1, brush, w, nullptr);
        }
    };
    const auto appendHorizontalLine = [&](const GridLineRange& r, FontDecorationPosition pos, ID2D1StrokeStyle* strokeStyle, const u32 color) {
        const auto from = r.from * scaledCellWidth;
        const auto to = r.to * scaledCellWidth;

        const auto brush = _brushWithColor(color);
        const f32 w = pos.height;
        const f32 centerY = cellCenter + pos.position + w * 0.5f;
        const D2D1_POINT_2F point0{ from, centerY };
        const D2D1_POINT_2F point1{ to, centerY };
        _renderTarget->DrawLine(point0, point1, brush, w, strokeStyle);
    };
    const auto appendCurlyLine = [&](const GridLineRange& r) {
        const auto& font = *p.s->font;

        const auto duTop = static_cast<f32>(font.doubleUnderline[0].position);
        const auto duBottom = static_cast<f32>(font.doubleUnderline[1].position);
        // The double-underline height is also our target line width.
        const auto duHeight = static_cast<f32>(font.doubleUnderline[0].height);

        // This gives it the same position and height as our double-underline. There's no particular reason for that, apart from
        // it being simple to implement and robust against more peculiar fonts with unusually large/small descenders, etc.
        // We still need to ensure though that it doesn't clip out of the cellHeight at the bottom, which is why `position` has a min().
        const auto height = std::max(3.0f, duBottom + duHeight - duTop);
        const auto position = std::min(duTop, cellHeight - height);

        // The amplitude of the wave needs to account for the stroke width, so that the final height including
        // antialiasing isn't larger than our target `height`. That's why we calculate `(height - duHeight)`.
        const auto center = cellCenter + position + 0.5f * height;
        const auto top = center - (height - duHeight);
        const auto bottom = center + (height - duHeight);
        const auto step = roundf(0.5f * height);
        const auto period = 4.0f * step;

        const auto from = r.from * scaledCellWidth;
        const auto to = r.to * scaledCellWidth;
        // Align the start of the wave to the nearest preceding period boundary.
        // This ensures that the wave is continuous across color and cell changes.
        auto x = floorf(from / period) * period;

        wil::com_ptr<ID2D1PathGeometry> geometry;
        THROW_IF_FAILED(p.d2dFactory->CreatePathGeometry(geometry.addressof()));

        wil::com_ptr<ID2D1GeometrySink> sink;
        THROW_IF_FAILED(geometry->Open(sink.addressof()));

        // This adds complete periods of the wave until we reach the end of the range.
        sink->BeginFigure({ x, center }, D2D1_FIGURE_BEGIN_HOLLOW);
        for (D2D1_QUADRATIC_BEZIER_SEGMENT segment; x < to;)
        {
            x += step;
            segment.point1.x = x;
            segment.point1.y = top;
            x += step;
            segment.point2.x = x;
            segment.point2.y = center;
            sink->AddQuadraticBezier(&segment);

            x += step;
            segment.point1.x = x;
            segment.point1.y = bottom;
            x += step;
            segment.point2.x = x;
            segment.point2.y = center;
            sink->AddQuadraticBezier(&segment);
        }
        sink->EndFigure(D2D1_FIGURE_END_OPEN);

        THROW_IF_FAILED(sink->Close());

        const auto brush = _brushWithColor(r.underlineColor);
        const D2D1_RECT_F clipRect{ from, rowTop, to, rowBottom };
        _renderTarget->PushAxisAlignedClip(&clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
        _renderTarget->DrawGeometry(geometry.get(), brush, duHeight, nullptr);
        _renderTarget->PopAxisAlignedClip();
    };

    for (const auto& r : row->gridLineRanges)
    {
        // AtlasEngine.cpp shouldn't add any gridlines if they don't do anything.
        assert(r.lines.any());

        if (r.lines.test(GridLines::Left))
        {
            appendVerticalLines(r, p.s->font->gridLeft);
        }
        if (r.lines.test(GridLines::Right))
        {
            appendVerticalLines(r, p.s->font->gridRight);
        }
        if (r.lines.test(GridLines::Top))
        {
            appendHorizontalLine(r, p.s->font->gridTop, nullptr, r.gridlineColor);
        }
        if (r.lines.test(GridLines::Bottom))
        {
            appendHorizontalLine(r, p.s->font->gridBottom, nullptr, r.gridlineColor);
        }
        if (r.lines.test(GridLines::Strikethrough))
        {
            appendHorizontalLine(r, p.s->font->strikethrough, nullptr, r.gridlineColor);
        }

        if (r.lines.test(GridLines::Underline))
        {
            appendHorizontalLine(r, p.s->font->underline, nullptr, r.underlineColor);
        }
        else if (r.lines.any(GridLines::DottedUnderline, GridLines::HyperlinkUnderline))
        {
            if (!_dottedStrokeStyle)
            {
                static constexpr D2D1_STROKE_STYLE_PROPERTIES props{ .dashStyle = D2D1_DASH_STYLE_CUSTOM };
                static constexpr FLOAT dashes[2]{ 1, 1 };
                THROW_IF_FAILED(p.d2dFactory->CreateStrokeStyle(&props, &dashes[0], 2, _dottedStrokeStyle.addressof()));
            }
            appendHorizontalLine(r, p.s->font->underline, _dottedStrokeStyle.get(), r.underlineColor);
        }
        else if (r.lines.test(GridLines::DashedUnderline))
        {
            if (!_dashedStrokeStyle)
            {
                static constexpr D2D1_STROKE_STYLE_PROPERTIES props{ .dashStyle = D2D1_DASH_STYLE_CUSTOM };
                static constexpr FLOAT dashes[2]{ 2, 2 };
                THROW_IF_FAILED(p.d2dFactory->CreateStrokeStyle(&props, &dashes[0], 2, _dashedStrokeStyle.addressof()));
            }
            appendHorizontalLine(r, p.s->font->underline, _dashedStrokeStyle.get(), r.underlineColor);
        }
        else if (r.lines.test(GridLines::CurlyUnderline))
        {
            appendCurlyLine(r);
        }
        else if (r.lines.test(GridLines::DoubleUnderline))
        {
            for (const auto pos : p.s->font->doubleUnderline)
            {
                appendHorizontalLine(r, pos, nullptr, r.underlineColor);
            }
        }
    }
}

void BackendD2D::_drawBitmap(const RenderingPayload& p, const ShapedRow* row, u16 y) const
{
    const auto& b = row->bitmap;

    // TODO: This could use some caching logic like BackendD3D.
    const D2D1_SIZE_U size{
        gsl::narrow_cast<UINT32>(b.sourceSize.x),
        gsl::narrow_cast<UINT32>(b.sourceSize.y),
    };
    const D2D1_BITMAP_PROPERTIES bitmapProperties{
        .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
        .dpiX = static_cast<f32>(p.s->font->dpi),
        .dpiY = static_cast<f32>(p.s->font->dpi),
    };
    wil::com_ptr<ID2D1Bitmap> bitmap;
    THROW_IF_FAILED(_renderTarget->CreateBitmap(size, b.source.data(), static_cast<UINT32>(b.sourceSize.x) * 4, &bitmapProperties, bitmap.addressof()));

    const i32 cellWidth = p.s->font->cellSize.x;
    const i32 cellHeight = p.s->font->cellSize.y;
    const auto left = (b.targetOffset - p.scrollOffsetX) * cellWidth;
    const auto right = left + b.targetWidth * cellWidth;
    const auto top = y * cellHeight;
    const auto bottom = top + cellHeight;

    const D2D1_RECT_F rectF{
        static_cast<f32>(left),
        static_cast<f32>(top),
        static_cast<f32>(right),
        static_cast<f32>(bottom),
    };
    _renderTarget->DrawBitmap(bitmap.get(), &rectF, 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void BackendD2D::_drawCursorPart1(const RenderingPayload& p)
{
    if (p.cursorRect.empty())
    {
        return;
    }

    const auto cursorColor = p.s->cursor->cursorColor;

    if (cursorColor != 0xffffffff)
    {
        const D2D1_RECT_F rect{
            static_cast<f32>(p.cursorRect.left * p.s->font->cellSize.x),
            static_cast<f32>(p.cursorRect.top * p.s->font->cellSize.y),
            static_cast<f32>(p.cursorRect.right * p.s->font->cellSize.x),
            static_cast<f32>(p.cursorRect.bottom * p.s->font->cellSize.y),
        };
        const auto brush = _brushWithColor(cursorColor);
        _drawCursor(p, _renderTarget.get(), rect, brush);
    }
}

void BackendD2D::_drawCursorPart2(const RenderingPayload& p)
{
    if (p.cursorRect.empty())
    {
        return;
    }

    if (p.s->cursor->cursorColor == 0xffffffff)
    {
        const auto cursorSize = p.cursorRect.size();
        if (cursorSize != _cursorBitmapSize)
        {
            _resizeCursorBitmap(p, cursorSize);
        }

        const D2D1_POINT_2F target{
            static_cast<f32>(p.cursorRect.left * p.s->font->cellSize.x),
            static_cast<f32>(p.cursorRect.top * p.s->font->cellSize.y),
        };
        _renderTarget->DrawImage(_cursorBitmap.get(), &target, nullptr, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_MASK_INVERT);
    }
}

void BackendD2D::_resizeCursorBitmap(const RenderingPayload& p, const til::size newSize)
{
    const til::size newSizeInPx{
        newSize.width * p.s->font->cellSize.x,
        newSize.height * p.s->font->cellSize.y,
    };

    // CreateCompatibleRenderTarget is a terrific API and does not adopt _any_ of the settings of the
    // parent render target (like the AA mode or D2D1_UNIT_MODE_PIXELS). Not sure who came up with that,
    // but fact is that we need to set both sizes to override the DPI and fake D2D1_UNIT_MODE_PIXELS.
    const D2D1_SIZE_F sizeF{ static_cast<f32>(newSizeInPx.width), static_cast<f32>(newSizeInPx.height) };
    const D2D1_SIZE_U sizeU{ gsl::narrow_cast<UINT32>(newSizeInPx.width), gsl::narrow_cast<UINT32>(newSizeInPx.height) };
    wil::com_ptr<ID2D1BitmapRenderTarget> cursorRenderTarget;
    THROW_IF_FAILED(_renderTarget->CreateCompatibleRenderTarget(&sizeF, &sizeU, nullptr, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, cursorRenderTarget.addressof()));

    cursorRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    cursorRenderTarget->BeginDraw();
    cursorRenderTarget->Clear();
    {
        const D2D1_RECT_F rect{ 0, 0, sizeF.width, sizeF.height };
        const auto brush = _brushWithColor(0xffffffff);
        _drawCursor(p, cursorRenderTarget.get(), rect, brush);
    }
    THROW_IF_FAILED(cursorRenderTarget->EndDraw());

    THROW_IF_FAILED(cursorRenderTarget->GetBitmap(_cursorBitmap.put()));
    _cursorBitmapSize = newSize;
}

void BackendD2D::_drawCursor(const RenderingPayload& p, ID2D1RenderTarget* renderTarget, D2D1_RECT_F rect, ID2D1Brush* brush) noexcept
{
    switch (static_cast<CursorType>(p.s->cursor->cursorType))
    {
    case CursorType::Legacy:
    {
        const auto height = p.s->cursor->heightPercentage / 100.0f;
        rect.top = roundf((rect.top - rect.bottom) * height + rect.bottom);
        renderTarget->FillRectangle(&rect, brush);
        break;
    }
    case CursorType::VerticalBar:
        rect.right = rect.left + p.s->font->thinLineWidth;
        renderTarget->FillRectangle(&rect, brush);
        break;
    case CursorType::Underscore:
        rect.top += p.s->font->underline.position;
        rect.bottom = rect.top + p.s->font->underline.height;
        renderTarget->FillRectangle(&rect, brush);
        break;
    case CursorType::EmptyBox:
    {
        const auto w = static_cast<f32>(p.s->font->thinLineWidth);
        const auto wh = w / 2.0f;
        rect.left += wh;
        rect.top += wh;
        rect.right -= wh;
        rect.bottom -= wh;
        renderTarget->DrawRectangle(&rect, brush, w, nullptr);
        break;
    }
    case CursorType::FullBox:
        renderTarget->FillRectangle(&rect, brush);
        break;
    case CursorType::DoubleUnderscore:
    {
        auto rect2 = rect;
        rect2.top = rect.top + p.s->font->doubleUnderline[0].position;
        rect2.bottom = rect2.top + p.s->font->thinLineWidth;
        renderTarget->FillRectangle(&rect2, brush);
        rect.top = rect.top + p.s->font->doubleUnderline[1].position;
        rect.bottom = rect.top + p.s->font->thinLineWidth;
        renderTarget->FillRectangle(&rect, brush);
        break;
    }
    default:
        break;
    }
}

#if ATLAS_DEBUG_SHOW_DIRTY
void BackendD2D::_debugShowDirty(const RenderingPayload& p)
{
    if (p.dirtyRectInPx.empty())
    {
        return;
    }

    _presentRects[_presentRectsPos] = p.dirtyRectInPx;
    _presentRectsPos = (_presentRectsPos + 1) % std::size(_presentRects);

    for (size_t i = 0; i < std::size(_presentRects); ++i)
    {
        const auto& rect = _presentRects[(_presentRectsPos + i) % std::size(_presentRects)];
        const D2D1_RECT_F rectF{
            static_cast<f32>(rect.left),
            static_cast<f32>(rect.top),
            static_cast<f32>(rect.right),
            static_cast<f32>(rect.bottom),
        };
        const auto color = til::colorbrewer::pastel1[i] | 0x1f000000;
        _fillRectangle(rectF, color);
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

    wil::com_ptr<ID3D11Texture2D> buffer;
    THROW_IF_FAILED(p.swapChain.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(buffer.addressof())));

    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\%u_%08zu.png", &_dumpRenderTargetBasePath[0], GetCurrentProcessId(), _dumpRenderTargetCounter);
    WIC::SaveTextureToPNG(p.deviceContext.get(), buffer.get(), p.s->font->dpi, &path[0]);
    _dumpRenderTargetCounter++;
}
#endif

ID2D1SolidColorBrush* BackendD2D::_brushWithColor(u32 color)
{
    if (_brushColor != color)
    {
        _brushWithColorUpdate(color);
    }
    return _brush.get();
}

ID2D1SolidColorBrush* BackendD2D::_brushWithColorUpdate(u32 color)
{
    const auto d2dColor = colorFromU32(color);
    _brush->SetColor(&d2dColor);
    _brushColor = color;
    return _brush.get();
}

void BackendD2D::_fillRectangle(const D2D1_RECT_F& rect, u32 color)
{
    const auto brush = _brushWithColor(color);
    _renderTarget->FillRectangle(&rect, brush);
}

TIL_FAST_MATH_END
