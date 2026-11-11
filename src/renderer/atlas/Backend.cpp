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

// Creates a bitmap of the image. Images larger than the maximum bitmap size of
// the device are downscaled, and `scale` receives the factor that was applied.
bool Microsoft::Console::Render::Atlas::CreateGraphicsBitmap(ID2D1DeviceContext* renderTarget, const GraphicsImage& image, ID2D1Bitmap** bitmap, f32x2& scale) noexcept
try
{
    const auto width = image.size.width;
    const auto height = image.size.height;
    if (width <= 0 || height <= 0 || image.pixels.size() < static_cast<size_t>(width) * height)
    {
        return false;
    }

    const auto maxSize = static_cast<f32>(renderTarget->GetMaximumBitmapSize());
    const auto factor = std::min(1.0f, maxSize / static_cast<f32>(std::max(width, height)));
    const auto scaledWidth = std::max(1, static_cast<i32>(static_cast<f32>(width) * factor));
    const auto scaledHeight = std::max(1, static_cast<i32>(static_cast<f32>(height) * factor));

    auto data = image.pixels.data();
    std::vector<u32> scaled;
    if (scaledWidth != width || scaledHeight != height)
    {
        scaled.resize(static_cast<size_t>(scaledWidth) * scaledHeight);
        for (i32 y = 0; y < scaledHeight; y++)
        {
            const auto src = &image.pixels[static_cast<size_t>(y) * height / scaledHeight * width];
            const auto dst = &scaled[static_cast<size_t>(y) * scaledWidth];
            for (i32 x = 0; x < scaledWidth; x++)
            {
                dst[x] = src[static_cast<size_t>(x) * width / scaledWidth];
            }
        }
        data = scaled.data();
    }

    const D2D1_SIZE_U size{ static_cast<UINT32>(scaledWidth), static_cast<UINT32>(scaledHeight) };
    const D2D1_BITMAP_PROPERTIES properties{
        .pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED },
        .dpiX = 96,
        .dpiY = 96,
    };
    if (FAILED_LOG(renderTarget->CreateBitmap(size, data, size.width * 4, &properties, bitmap)))
    {
        return false;
    }
    scale = { static_cast<f32>(scaledWidth) / static_cast<f32>(width), static_cast<f32>(scaledHeight) / static_cast<f32>(height) };
    return true;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}
