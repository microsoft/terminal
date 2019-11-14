// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/RenderEngineBase.hpp"

#include <functional>

#include <dxgi.h>
#include <dxgi1_2.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <wrl.h>
#include <wrl/client.h>

#include "CustomTextRenderer.h"

#include "../../types/inc/Viewport.hpp"

namespace Microsoft::Console::Render
{
    class DxEngine final : public RenderEngineBase
    {
    public:
        DxEngine();
        ~DxEngine();
        DxEngine(const DxEngine&) = default;
        DxEngine(DxEngine&&) = default;
        DxEngine& operator=(const DxEngine&) = default;
        DxEngine& operator=(DxEngine&&) = default;

        // Used to release device resources so that another instance of
        // conhost can render to the screen (i.e. only one DirectX
        // application may control the screen at a time.)
        [[nodiscard]] HRESULT Enable() noexcept;
        [[nodiscard]] HRESULT Disable() noexcept;

        [[nodiscard]] HRESULT SetHwnd(const HWND hwnd) noexcept;

        [[nodiscard]] HRESULT SetWindowSize(const SIZE pixels) noexcept;

        void SetCallback(std::function<void()> pfn);

        ::Microsoft::WRL::ComPtr<IDXGISwapChain1> GetSwapChain();

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                              COORD const coord,
                                              bool const fTrimLeft) noexcept override;

        [[nodiscard]] HRESULT PaintBufferGridLines(GridLines const lines, COLORREF const color, size_t const cchLine, COORD const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(COLORREF const colorForeground,
                                                   COLORREF const colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   bool const isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int const iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, int const iDpi) noexcept override;

        [[nodiscard]] SMALL_RECT GetDirtyRectInChars() noexcept override;

        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

        [[nodiscard]] ::Microsoft::Console::Types::Viewport GetViewportInCharacters(const ::Microsoft::Console::Types::Viewport& viewInPixels) noexcept;

        float GetScaling() const noexcept;

        void SetSelectionBackground(const COLORREF color) noexcept;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring& newTitle) noexcept override;

    private:
        enum class SwapChainMode
        {
            ForHwnd,
            ForComposition
        };

        SwapChainMode _chainMode;

        HWND _hwndTarget;
        SIZE _sizeTarget;
        int _dpi;
        float _scale;

        std::function<void()> _pfn;

        bool _isEnabled;
        bool _isPainting;

        SIZE _displaySizePixels;
        SIZE _glyphCell;

        D2D1_COLOR_F _defaultForegroundColor;
        D2D1_COLOR_F _defaultBackgroundColor;

        D2D1_COLOR_F _foregroundColor;
        D2D1_COLOR_F _backgroundColor;
        D2D1_COLOR_F _selectionBackground;

        [[nodiscard]] RECT _GetDisplayRect() const noexcept;

        bool _isInvalidUsed;
        RECT _invalidRect;
        SIZE _invalidScroll;

        void _InvalidOr(SMALL_RECT sr) noexcept;
        void _InvalidOr(RECT rc) noexcept;

        void _InvalidOffset(POINT pt);

        bool _presentReady;
        RECT _presentDirty;
        RECT _presentScroll;
        POINT _presentOffset;
        DXGI_PRESENT_PARAMETERS _presentParams;

        static const ULONG s_ulMinCursorHeightPercent = 25;
        static const ULONG s_ulMaxCursorHeightPercent = 100;

        // Device-Independent Resources
        ::Microsoft::WRL::ComPtr<ID2D1Factory> _d2dFactory;
        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormat;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFace;
        ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _dwriteTextAnalyzer;
        ::Microsoft::WRL::ComPtr<CustomTextRenderer> _customRenderer;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _strokeStyle;

        // Device-Dependent Resources
        bool _haveDeviceResources;
        ::Microsoft::WRL::ComPtr<ID3D11Device> _d3dDevice;
        ::Microsoft::WRL::ComPtr<ID3D11DeviceContext> _d3dDeviceContext;
        ::Microsoft::WRL::ComPtr<IDXGIFactory2> _dxgiFactory2;
        ::Microsoft::WRL::ComPtr<IDXGISurface> _dxgiSurface;
        ::Microsoft::WRL::ComPtr<ID2D1RenderTarget> _d2dRenderTarget;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushForeground;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushBackground;
        ::Microsoft::WRL::ComPtr<IDXGISwapChain1> _dxgiSwapChain;

        [[nodiscard]] HRESULT _CreateDeviceResources(const bool createSwapChain) noexcept;

        [[nodiscard]] HRESULT _PrepareRenderTarget() noexcept;

        void _ReleaseDeviceResources() noexcept;

        [[nodiscard]] HRESULT _CreateTextLayout(
            _In_reads_(StringLength) PCWCHAR String,
            _In_ size_t StringLength,
            _Out_ IDWriteTextLayout** ppTextLayout) noexcept;

        [[nodiscard]] HRESULT _CopyFrontToBack() noexcept;

        [[nodiscard]] HRESULT _EnableDisplayAccess(const bool outputEnabled) noexcept;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                              DWRITE_FONT_WEIGHT& weight,
                                                                                              DWRITE_FONT_STRETCH& stretch,
                                                                                              DWRITE_FONT_STYLE& style,
                                                                                              std::wstring& localeName) const;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(std::wstring& familyName,
                                                                               DWRITE_FONT_WEIGHT& weight,
                                                                               DWRITE_FONT_STRETCH& stretch,
                                                                               DWRITE_FONT_STYLE& style,
                                                                               std::wstring& localeName) const;

        [[nodiscard]] std::wstring _GetLocaleName() const;

        [[nodiscard]] std::wstring _GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                      std::wstring& localeName) const;

        [[nodiscard]] HRESULT _GetProposedFont(const FontInfoDesired& desired,
                                               FontInfo& actual,
                                               const int dpi,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormat,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1>& textAnalyzer,
                                               ::Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFace) const noexcept;

        [[nodiscard]] COORD _GetFontSize() const noexcept;

        [[nodiscard]] SIZE _GetClientSize() const noexcept;

        [[nodiscard]] D2D1_COLOR_F _ColorFFromColorRef(const COLORREF color) noexcept;

        // Routine Description:
        // - Helps convert a Direct2D ColorF into a DXGI RGBA
        // Arguments:
        // - color - Direct2D Color F
        // Return Value:
        // - DXGI RGBA
        [[nodiscard]] constexpr DXGI_RGBA s_RgbaFromColorF(const D2D1_COLOR_F color) noexcept
        {
            return { color.r, color.g, color.b, color.a };
        }
    };
}
