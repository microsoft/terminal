#pragma once

#include "Backend.h"

namespace Microsoft::Console::Render::Atlas
{
    struct BackendD2D : IBackend
    {
        BackendD2D(wil::com_ptr<ID3D11Device2> device, wil::com_ptr<ID3D11DeviceContext2> deviceContext) noexcept;

        void Render(RenderingPayload& payload) override;
        bool RequiresContinuousRedraw() noexcept override;
        void WaitUntilCanRender() noexcept override;

    private:
        __declspec(noinline) void _handleSettingsUpdate(const RenderingPayload& p);
        void _drawBackground(const RenderingPayload& p) noexcept;
        void _drawText(RenderingPayload& p);
        void _drawGridlines(const RenderingPayload& p);
        void _drawGridlineRow(const RenderingPayload& p, const ShapedRow& row, u16 y);
        void _drawCursor(const RenderingPayload& p);
        void _drawSelection(const RenderingPayload& p);
        ID2D1Brush* _brushWithColor(u32 color);
        void _fillRectangle(const D2D1_RECT_F& rect, u32 color);

        SwapChainManager _swapChainManager;

        wil::com_ptr<ID3D11Device2> _device;
        wil::com_ptr<ID3D11DeviceContext2> _deviceContext;

        wil::com_ptr<ID2D1DeviceContext> _renderTarget;
        wil::com_ptr<ID2D1DeviceContext4> _renderTarget4; // Optional. Supported since Windows 10 14393.
        wil::com_ptr<ID2D1SolidColorBrush> _brush;
        wil::com_ptr<ID2D1StrokeStyle> _dottedStrokeStyle;
        wil::com_ptr<ID2D1Bitmap> _backgroundBitmap;
        wil::com_ptr<ID2D1BitmapBrush> _backgroundBrush;

        u32 _brushColor = 0;

        til::generation_t _generation;
        til::generation_t _fontGeneration;
        u16x2 _cellCount;
    };
}
