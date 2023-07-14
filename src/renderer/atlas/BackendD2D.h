// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/flat_set.h>

#include "Backend.h"

namespace Microsoft::Console::Render::Atlas
{
    struct BackendD2D : IBackend
    {
        void ReleaseResources() noexcept override;
        void Render(RenderingPayload& payload) override;
        bool RequiresContinuousRedraw() noexcept override;

    private:
        ATLAS_ATTR_COLD void _handleSettingsUpdate(const RenderingPayload& p);
        void _drawBackground(const RenderingPayload& p) noexcept;
        void _drawText(RenderingPayload& p);
        ATLAS_ATTR_COLD f32 _drawTextPrepareLineRendition(const RenderingPayload& p, const ShapedRow* row, f32 baselineY) const noexcept;
        ATLAS_ATTR_COLD void _drawTextResetLineRendition(const ShapedRow* row) const noexcept;
        ATLAS_ATTR_COLD f32r _getGlyphRunDesignBounds(const DWRITE_GLYPH_RUN& glyphRun, f32 baselineX, f32 baselineY);
        ATLAS_ATTR_COLD void _drawGridlineRow(const RenderingPayload& p, const ShapedRow* row, u16 y);
        void _drawCursorPart1(const RenderingPayload& p);
        void _drawCursorPart2(const RenderingPayload& p);
        static void _drawCursor(const RenderingPayload& p, ID2D1RenderTarget* renderTarget, D2D1_RECT_F rect, ID2D1Brush* brush) noexcept;
        void _resizeCursorBitmap(const RenderingPayload& p, til::size newSize);
        void _drawSelection(const RenderingPayload& p);
        void _debugShowDirty(const RenderingPayload& p);
        void _debugDumpRenderTarget(const RenderingPayload& p);
        ID2D1SolidColorBrush* _brushWithColor(u32 color);
        ATLAS_ATTR_COLD ID2D1SolidColorBrush* _brushWithColorUpdate(u32 color);
        void _fillRectangle(const D2D1_RECT_F& rect, u32 color);

        wil::com_ptr<ID2D1DeviceContext> _renderTarget;
        wil::com_ptr<ID2D1DeviceContext4> _renderTarget4; // Optional. Supported since Windows 10 14393.
        wil::com_ptr<ID2D1StrokeStyle> _dottedStrokeStyle;
        wil::com_ptr<ID2D1Bitmap> _backgroundBitmap;
        wil::com_ptr<ID2D1BitmapBrush> _backgroundBrush;
        til::generation_t _backgroundBitmapGeneration;

        wil::com_ptr<ID2D1Bitmap> _cursorBitmap;
        til::size _cursorBitmapSize; // in columns/rows

        wil::com_ptr<ID2D1SolidColorBrush> _emojiBrush;
        wil::com_ptr<ID2D1SolidColorBrush> _brush;
        u32 _brushColor = 0;

        Buffer<DWRITE_GLYPH_METRICS> _glyphMetrics;

        til::generation_t _generation;
        til::generation_t _fontGeneration;
        til::generation_t _cursorGeneration;
        u16x2 _viewportCellCount{};

#if ATLAS_DEBUG_SHOW_DIRTY
        i32r _presentRects[9]{};
        size_t _presentRectsPos = 0;
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
        wchar_t _dumpRenderTargetBasePath[MAX_PATH]{};
        size_t _dumpRenderTargetCounter = 0;
#endif
    };
}
