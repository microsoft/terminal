// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/Cluster.hpp"

#include <vector>
#include <d2d1_1.h>
#include <wrl.h>
#include <wrl/client.h>

namespace Microsoft::Console::Render
{
    struct DrawingContext;

    class DxSoftFont
    {
    public:
        void SetFont(const std::span<const uint16_t> bitPattern,
                     const til::size sourceSize,
                     const til::size targetSize,
                     const size_t centeringHint);
        HRESULT SetTargetSize(const til::size targetSize);
        HRESULT SetAntialiasing(const bool antialiased);
        HRESULT SetColor(const D2D1_COLOR_F& color);
        HRESULT Draw(const DrawingContext& drawingContext,
                     const std::span<const Cluster> clusters,
                     const float originX,
                     const float originY);
        void Reset();

    private:
        static constexpr auto ANTIALIASED_INTERPOLATION = D2D1_SCALE_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
        static constexpr auto ALIASED_INTERPOLATION = D2D1_SCALE_INTERPOLATION_MODE_NEAREST_NEIGHBOR;

        HRESULT _createResources(gsl::not_null<ID2D1DeviceContext*> d2dContext);
        D2D1_VECTOR_2F _scaleForTargetSize() const noexcept;
        template<typename T>
        T _xOffsetForGlyph(const size_t glyphNumber) const noexcept;
        template<typename T>
        T _yOffsetForGlyph(const size_t glyphNumber) const noexcept;

        size_t _glyphCount = 0;
        til::size _sourceSize;
        til::size _targetSize;
        size_t _centeringHint = 0;
        D2D1_SCALE_INTERPOLATION_MODE _interpolation = ALIASED_INTERPOLATION;
        D2D1_MATRIX_5X4_F _colorMatrix{ ._14 = 1 };
        D2D1_SIZE_U _bitmapSize{};
        std::vector<byte> _bitmapBits;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap> _bitmap;
        ::Microsoft::WRL::ComPtr<ID2D1Effect> _scaleEffect;
        ::Microsoft::WRL::ComPtr<ID2D1Effect> _colorEffect;
    };
}
