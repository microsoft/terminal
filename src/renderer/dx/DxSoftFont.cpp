// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DxSoftFont.h"
#include "CustomTextRenderer.h"

#include <d2d1effects.h>

#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::Console::Render;
using Microsoft::WRL::ComPtr;

// The soft font is rendered into a bitmap laid out in a 12x8 grid, which is
// enough space for the 96 characters expected in the font, and which minimizes
// the dimensions for a typical 2:1 cell size. Each position in the grid is
// surrounded by a 2 pixel border which helps avoid bleed across the character
// boundaries when the output is scaled.

constexpr size_t BITMAP_GRID_WIDTH = 12;
constexpr size_t BITMAP_GRID_HEIGHT = 8;
constexpr size_t PADDING = 2;

void DxSoftFont::SetFont(const std::span<const uint16_t> bitPattern,
                         const til::size sourceSize,
                         const til::size targetSize,
                         const size_t centeringHint)
{
    Reset();

    // If the font is being reset, just free up the memory and return.
    if (bitPattern.empty())
    {
        _bitmapBits.clear();
        return;
    }

    const auto maxGlyphCount = BITMAP_GRID_WIDTH * BITMAP_GRID_HEIGHT;
    _glyphCount = std::min<size_t>(bitPattern.size() / sourceSize.height, maxGlyphCount);
    _sourceSize = sourceSize;
    _targetSize = targetSize;
    _centeringHint = centeringHint;

    const auto bitmapWidth = BITMAP_GRID_WIDTH * (_sourceSize.width + PADDING * 2);
    const auto bitmapHeight = BITMAP_GRID_HEIGHT * (_sourceSize.height + PADDING * 2);
    _bitmapBits = std::vector<byte>(bitmapWidth * bitmapHeight);
    _bitmapSize = { gsl::narrow_cast<UINT32>(bitmapWidth), gsl::narrow_cast<UINT32>(bitmapHeight) };

    const auto bitmapScanline = [=](const auto lineNumber) noexcept {
        return _bitmapBits.begin() + lineNumber * bitmapWidth;
    };

    // The source bitPattern is just a list of the scanlines making up the
    // glyphs one after the other, but we want to lay them out in a grid, so
    // we need to process each glyph individually.
    auto srcPointer = bitPattern.begin();
    for (auto glyphNumber = 0u; glyphNumber < _glyphCount; glyphNumber++)
    {
        // We start by calculating the position in the bitmap where the glyph
        // needs to be stored.
        const auto xOffset = _xOffsetForGlyph<size_t>(glyphNumber);
        const auto yOffset = _yOffsetForGlyph<size_t>(glyphNumber);
        auto dstPointer = bitmapScanline(yOffset) + xOffset;
        for (auto y = 0; y < sourceSize.height; y++)
        {
            // Then for each scanline in the source, we need to expand the bits
            // into 8-bit values. For every bit that is set we write out an FF
            // value, and if not set, we write out 00. In the end, all we care
            // about is a single red component for the R8_UNORM bitmap format,
            // since we'll later remap that to RGBA with a color matrix.
            auto srcBits = *(srcPointer++);
            for (auto x = 0; x < sourceSize.width; x++)
            {
                const auto srcBitIsSet = (srcBits & 0x8000) != 0;
                *(dstPointer++) = srcBitIsSet ? 0xFF : 0x00;
                srcBits <<= 1;
            }
            // When glyphs in this bitmap are output, they will typically need
            // to scaled, and this can result in some bleed from the surrounding
            // pixels. So to keep the borders clean, we pad the areas to the left
            // and right by repeating the first and last pixels of each scanline.
            std::fill_n(dstPointer, PADDING, til::at(dstPointer, -1));
            dstPointer -= sourceSize.width;
            std::fill_n(dstPointer - PADDING, PADDING, til::at(dstPointer, 0));
            dstPointer += bitmapWidth;
        }
    }

    // In the same way that we padded the left and right of each glyph in the
    // code above, we also need to pad the top and bottom. But in this case we
    // can simply do a whole row of glyphs from the grid at the same time.
    for (auto gridRow = 0u; gridRow < BITMAP_GRID_HEIGHT; gridRow++)
    {
        const auto rowOffset = _yOffsetForGlyph<size_t>(gridRow);
        const auto rowTop = bitmapScanline(rowOffset);
        const auto rowBottom = bitmapScanline(rowOffset + _sourceSize.height - 1);
        for (auto i = 1; i <= PADDING; i++)
        {
            std::copy_n(rowTop, bitmapWidth, rowTop - i * bitmapWidth);
            std::copy_n(rowBottom, bitmapWidth, rowBottom + i * bitmapWidth);
        }
    }
}

HRESULT DxSoftFont::SetTargetSize(const til::size targetSize)
{
    _targetSize = targetSize;
    return _scaleEffect ? _scaleEffect->SetValue(D2D1_SCALE_PROP_SCALE, _scaleForTargetSize()) : S_OK;
}

HRESULT DxSoftFont::SetAntialiasing(const bool antialiased)
{
    _interpolation = (antialiased ? ANTIALIASED_INTERPOLATION : ALIASED_INTERPOLATION);
    return _scaleEffect ? _scaleEffect->SetValue(D2D1_SCALE_PROP_INTERPOLATION_MODE, _interpolation) : S_OK;
}

HRESULT DxSoftFont::SetColor(const D2D1_COLOR_F& color)
{
    // Since our source image is monochrome, we don't care about the
    // individual color components. We just multiply the red component
    // by the active color value to get the output color. And note that
    // the alpha matrix entry is already set to 1 in the constructor,
    // so we don't need to keep updating it here.
    _colorMatrix.m[0][0] = color.r;
    _colorMatrix.m[0][1] = color.g;
    _colorMatrix.m[0][2] = color.b;
    return _colorEffect ? _colorEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, _colorMatrix) : S_OK;
}

HRESULT DxSoftFont::Draw(const DrawingContext& drawingContext,
                         const std::span<const Cluster> clusters,
                         const float originX,
                         const float originY)
{
    ComPtr<ID2D1DeviceContext> d2dContext;
    RETURN_IF_FAILED(drawingContext.renderTarget->QueryInterface(d2dContext.GetAddressOf()));

    // We start by creating a clipping rectangle for the region we're going to
    // draw, and this is initially filled with the active background color.
    D2D1_RECT_F rect;
    rect.top = originY + drawingContext.topClipOffset;
    rect.bottom = originY + _targetSize.height - drawingContext.bottomClipOffset;
    rect.left = originX;
    rect.right = originX + _targetSize.width * clusters.size();
    d2dContext->FillRectangle(rect, drawingContext.backgroundBrush);
    d2dContext->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
    auto resetClippingRect = wil::scope_exit([&]() noexcept { d2dContext->PopAxisAlignedClip(); });

    // The bitmap and associated scaling/coloring effects are created on demand
    // so we need make sure they're generated now.
    RETURN_IF_FAILED(_createResources(d2dContext.Get()));

    // We use the CustomTextRenderer to draw the first pass of the cursor.
    RETURN_IF_FAILED(CustomTextRenderer::DrawCursor(d2dContext.Get(), rect, drawingContext, true));

    // Then we draw the associated glyph for each entry in the cluster list.
    auto targetPoint = D2D1_POINT_2F{ originX, originY };
    for (auto& cluster : clusters)
    {
        // For DRCS, we only care about the character's lower 7 bits, then
        // codepoint 0x20 will be the first glyph in the set.
        const auto glyphNumber = (cluster.GetTextAsSingle() & 0x7f) - 0x20;
        const auto x = _xOffsetForGlyph<float>(glyphNumber);
        const auto y = _yOffsetForGlyph<float>(glyphNumber);
        const auto sourceRect = D2D1_RECT_F{ x, y, x + _targetSize.width, y + _targetSize.height };
        LOG_IF_FAILED(_scaleEffect->SetValue(D2D1_SCALE_PROP_CENTER_POINT, D2D1::Point2F(x, y)));
        d2dContext->DrawImage(_colorEffect.Get(), targetPoint, sourceRect);
        targetPoint.x += _targetSize.width;
    }

    // We finish by the drawing the second pass of the cursor.
    return CustomTextRenderer::DrawCursor(d2dContext.Get(), rect, drawingContext, false);
}

void DxSoftFont::Reset()
{
    _colorEffect.Reset();
    _scaleEffect.Reset();
    _bitmap.Reset();
}

HRESULT DxSoftFont::_createResources(gsl::not_null<ID2D1DeviceContext*> d2dContext)
{
    if (!_bitmap)
    {
        D2D1_BITMAP_PROPERTIES bitmapProperties{};
        bitmapProperties.pixelFormat.format = DXGI_FORMAT_R8_UNORM;
        bitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        const auto bitmapPitch = gsl::narrow_cast<UINT32>(_bitmapSize.width);
        RETURN_IF_FAILED(d2dContext->CreateBitmap(_bitmapSize, _bitmapBits.data(), bitmapPitch, bitmapProperties, _bitmap.GetAddressOf()));
    }

    if (!_scaleEffect)
    {
        RETURN_IF_FAILED(d2dContext->CreateEffect(CLSID_D2D1Scale, _scaleEffect.GetAddressOf()));
        RETURN_IF_FAILED(_scaleEffect->SetValue(D2D1_SCALE_PROP_INTERPOLATION_MODE, _interpolation));
        RETURN_IF_FAILED(_scaleEffect->SetValue(D2D1_SCALE_PROP_SCALE, _scaleForTargetSize()));
        _scaleEffect->SetInput(0, _bitmap.Get());

        if (_colorEffect)
        {
            _colorEffect->SetInputEffect(0, _scaleEffect.Get());
        }
    }

    if (!_colorEffect)
    {
        RETURN_IF_FAILED(d2dContext->CreateEffect(CLSID_D2D1ColorMatrix, _colorEffect.GetAddressOf()));
        RETURN_IF_FAILED(_colorEffect->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, _colorMatrix));
        _colorEffect->SetInputEffect(0, _scaleEffect.Get());
    }

    return S_OK;
}

D2D1_VECTOR_2F DxSoftFont::_scaleForTargetSize() const noexcept
{
    // If the text in the font is not perfectly centered, the _centeringHint
    // gives us the offset needed to correct that misalignment. So to ensure
    // the scaling is evenly balanced around the center point of the glyphs,
    // we can use that hint to adjust the dimensions of our source and target
    // widths when calculating the horizontal scale.
    const auto targetCenteringHint = std::lround((float)_centeringHint * _targetSize.width / _sourceSize.width);
    const auto xScale = gsl::narrow_cast<float>(_targetSize.width - targetCenteringHint) / (_sourceSize.width - _centeringHint);
    const auto yScale = gsl::narrow_cast<float>(_targetSize.height) / _sourceSize.height;
    return D2D1::Vector2F(xScale, yScale);
}

template<typename T>
T DxSoftFont::_xOffsetForGlyph(const size_t glyphNumber) const noexcept
{
    const auto xOffsetInGrid = glyphNumber / BITMAP_GRID_HEIGHT;
    const auto paddedGlyphWidth = _sourceSize.width + PADDING * 2;
    return gsl::narrow_cast<T>(xOffsetInGrid * paddedGlyphWidth + PADDING);
}

template<typename T>
T DxSoftFont::_yOffsetForGlyph(const size_t glyphNumber) const noexcept
{
    const auto yOffsetInGrid = glyphNumber % BITMAP_GRID_HEIGHT;
    const auto paddedGlyphHeight = _sourceSize.height + PADDING * 2;
    return gsl::narrow_cast<T>(yOffsetInGrid * paddedGlyphHeight + PADDING);
}
