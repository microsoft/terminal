// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Backend.h"

TIL_FAST_MATH_BEGIN

// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

void Microsoft::Console::Render::Atlas::GlyphRunAccumulateBounds(const ID2D1DeviceContext* d2dRenderTarget, D2D1_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun, D2D1_RECT_F& bounds)
{
    D2D1_RECT_F rect{};
    THROW_IF_FAILED(d2dRenderTarget->GetGlyphRunWorldBounds(baselineOrigin, glyphRun, DWRITE_MEASURING_MODE_NATURAL, &rect));
    if (rect.top < rect.bottom)
    {
        bounds.left = std::min(bounds.left, rect.left);
        bounds.top = std::min(bounds.top, rect.top);
        bounds.right = std::max(bounds.right, rect.right);
        bounds.bottom = std::max(bounds.bottom, rect.bottom);
    }
}

wil::com_ptr<IDWriteColorGlyphRunEnumerator1> Microsoft::Console::Render::Atlas::TranslateColorGlyphRun(IDWriteFactory4* dwriteFactory4, D2D_POINT_2F baselineOrigin, const DWRITE_GLYPH_RUN* glyphRun) noexcept
{
    static constexpr auto formats =
        DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
        DWRITE_GLYPH_IMAGE_FORMATS_CFF |
        DWRITE_GLYPH_IMAGE_FORMATS_COLR |
        DWRITE_GLYPH_IMAGE_FORMATS_SVG |
        DWRITE_GLYPH_IMAGE_FORMATS_PNG |
        DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
        DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
        DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;

    wil::com_ptr<IDWriteColorGlyphRunEnumerator1> enumerator;

    if (dwriteFactory4)
    {
        std::ignore = dwriteFactory4->TranslateColorGlyphRun(baselineOrigin, glyphRun, nullptr, formats, DWRITE_MEASURING_MODE_NATURAL, nullptr, 0, enumerator.addressof());
    }

    return enumerator;
}

bool Microsoft::Console::Render::Atlas::ColorGlyphRunMoveNext(IDWriteColorGlyphRunEnumerator1* enumerator)
{
    BOOL hasRun;
    THROW_IF_FAILED(enumerator->MoveNext(&hasRun));
    return hasRun;
}

const DWRITE_COLOR_GLYPH_RUN1* Microsoft::Console::Render::Atlas::ColorGlyphRunGetCurrentRun(IDWriteColorGlyphRunEnumerator1* enumerator)
{
    const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun = nullptr;
    THROW_IF_FAILED(enumerator->GetCurrentRun(&colorGlyphRun));
    return colorGlyphRun;
}

void Microsoft::Console::Render::Atlas::ColorGlyphRunAccumulateBounds(const ID2D1DeviceContext* d2dRenderTarget, const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun, D2D1_RECT_F& bounds)
{
    const D2D1_POINT_2F baselineOrigin{ colorGlyphRun->baselineOriginX, colorGlyphRun->baselineOriginY };
    GlyphRunAccumulateBounds(d2dRenderTarget, baselineOrigin, &colorGlyphRun->glyphRun, bounds);
}

void Microsoft::Console::Render::Atlas::ColorGlyphRunDraw(ID2D1DeviceContext4* d2dRenderTarget4, ID2D1SolidColorBrush* emojiBrush, ID2D1SolidColorBrush* foregroundBrush, const DWRITE_COLOR_GLYPH_RUN1* colorGlyphRun) noexcept
{
    ID2D1Brush* runBrush = nullptr;
    if (colorGlyphRun->paletteIndex == /*DWRITE_NO_PALETTE_INDEX*/ 0xffff)
    {
        runBrush = foregroundBrush;
    }
    else
    {
        emojiBrush->SetColor(&colorGlyphRun->runColor);
        runBrush = emojiBrush;
    }

    const D2D1_POINT_2F baselineOrigin{ colorGlyphRun->baselineOriginX, colorGlyphRun->baselineOriginY };

    switch (colorGlyphRun->glyphImageFormat)
    {
    case DWRITE_GLYPH_IMAGE_FORMATS_NONE:
        break;
    case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
    case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
    case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
    case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
        d2dRenderTarget4->DrawColorBitmapGlyphRun(colorGlyphRun->glyphImageFormat, baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
        break;
    case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
        d2dRenderTarget4->DrawSvgGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, runBrush, nullptr, 0, colorGlyphRun->measuringMode);
        break;
    default:
        d2dRenderTarget4->DrawGlyphRun(baselineOrigin, &colorGlyphRun->glyphRun, colorGlyphRun->glyphRunDescription, runBrush, colorGlyphRun->measuringMode);
        break;
    }
}

TIL_FAST_MATH_END
