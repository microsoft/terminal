// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wrl/implements.h>
#include "BoxDrawingEffect.h"
#include "../../renderer/inc/CursorOptions.h"

namespace Microsoft::Console::Render
{
    struct DrawingContext
    {
        DrawingContext(ID2D1RenderTarget* renderTarget,
                       ID2D1SolidColorBrush* foregroundBrush,
                       ID2D1SolidColorBrush* backgroundBrush,
                       bool forceGrayscaleAA,
                       IDWriteFactory* dwriteFactory,
                       const DWRITE_LINE_SPACING spacing,
                       const D2D_SIZE_F cellSize,
                       const D2D_SIZE_F targetSize,
                       const std::optional<CursorOptions>& cursorInfo,
                       const D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE) noexcept :
            renderTarget(renderTarget),
            foregroundBrush(foregroundBrush),
            backgroundBrush(backgroundBrush),
            useBoldFont(false),
            useItalicFont(false),
            forceGrayscaleAA(forceGrayscaleAA),
            dwriteFactory(dwriteFactory),
            spacing(spacing),
            cellSize(cellSize),
            targetSize(targetSize),
            cursorInfo(cursorInfo),
            options(options),
            topClipOffset(0),
            bottomClipOffset(0)
        {
        }

        ID2D1RenderTarget* renderTarget;
        ID2D1SolidColorBrush* foregroundBrush;
        ID2D1SolidColorBrush* backgroundBrush;
        bool useBoldFont;
        bool useItalicFont;
        bool forceGrayscaleAA;
        IDWriteFactory* dwriteFactory;
        DWRITE_LINE_SPACING spacing;
        D2D_SIZE_F cellSize;
        D2D_SIZE_F targetSize;
        std::optional<CursorOptions> cursorInfo;
        D2D1_DRAW_TEXT_OPTIONS options;
        FLOAT topClipOffset;
        FLOAT bottomClipOffset;
    };

    // Helper to choose which Direct2D method to use when drawing the cursor rectangle
    enum class CursorPaintType
    {
        Fill,
        Outline
    };

    constexpr const ULONG MinCursorHeightPixels = 1;
    constexpr const ULONG MinCursorHeightPercent = 1;
    constexpr const ULONG MaxCursorHeightPercent = 100;

    class CustomTextRenderer : public ::Microsoft::WRL::RuntimeClass<::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom | ::Microsoft::WRL::InhibitFtmBase>, IDWriteTextRenderer>
    {
    public:
        // http://www.charlespetzold.com/blog/2014/01/Character-Formatting-Extensions-with-DirectWrite.html
        // https://docs.microsoft.com/en-us/windows/desktop/DirectWrite/how-to-implement-a-custom-text-renderer

        // IDWritePixelSnapping methods
        [[nodiscard]] HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void* clientDrawingContext,
                                                                        _Out_ BOOL* isDisabled) noexcept override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void* clientDrawingContext,
                                                                _Out_ FLOAT* pixelsPerDip) noexcept override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetCurrentTransform(void* clientDrawingContext,
                                                                    _Out_ DWRITE_MATRIX* transform) noexcept override;

        // IDWriteTextRenderer methods
        [[nodiscard]] HRESULT STDMETHODCALLTYPE DrawGlyphRun(void* clientDrawingContext,
                                                             FLOAT baselineOriginX,
                                                             FLOAT baselineOriginY,
                                                             DWRITE_MEASURING_MODE measuringMode,
                                                             _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                             _In_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                                             IUnknown* clientDrawingEffect) override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE DrawUnderline(void* clientDrawingContext,
                                                              FLOAT baselineOriginX,
                                                              FLOAT baselineOriginY,
                                                              _In_ const DWRITE_UNDERLINE* underline,
                                                              IUnknown* clientDrawingEffect) noexcept override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE DrawStrikethrough(void* clientDrawingContext,
                                                                  FLOAT baselineOriginX,
                                                                  FLOAT baselineOriginY,
                                                                  _In_ const DWRITE_STRIKETHROUGH* strikethrough,
                                                                  IUnknown* clientDrawingEffect) noexcept override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE DrawInlineObject(void* clientDrawingContext,
                                                                 FLOAT originX,
                                                                 FLOAT originY,
                                                                 IDWriteInlineObject* inlineObject,
                                                                 BOOL isSideways,
                                                                 BOOL isRightToLeft,
                                                                 IUnknown* clientDrawingEffect) noexcept override;

        [[nodiscard]] HRESULT STDMETHODCALLTYPE EndClip(void* clientDrawingContext) noexcept;

        [[nodiscard]] static HRESULT DrawCursor(gsl::not_null<ID2D1DeviceContext*> d2dContext,
                                                D2D1_RECT_F textRunBounds,
                                                const DrawingContext& drawingContext,
                                                const bool firstPass);

    private:
        [[nodiscard]] HRESULT _FillRectangle(void* clientDrawingContext,
                                             IUnknown* clientDrawingEffect,
                                             float x,
                                             float y,
                                             float width,
                                             float thickness,
                                             DWRITE_READING_DIRECTION readingDirection,
                                             DWRITE_FLOW_DIRECTION flowDirection) noexcept;

        [[nodiscard]] HRESULT _DrawBasicGlyphRun(DrawingContext* clientDrawingContext,
                                                 D2D1_POINT_2F baselineOrigin,
                                                 DWRITE_MEASURING_MODE measuringMode,
                                                 _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                 _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                                 ID2D1Brush* brush,
                                                 _In_opt_ IUnknown* clientDrawingEffect);

        [[nodiscard]] HRESULT _DrawBoxRunManually(DrawingContext* clientDrawingContext,
                                                  D2D1_POINT_2F baselineOrigin,
                                                  DWRITE_MEASURING_MODE measuringMode,
                                                  _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                  _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                                  _In_ IBoxDrawingEffect* clientDrawingEffect) noexcept;

        [[nodiscard]] HRESULT _DrawGlowGlyphRun(DrawingContext* clientDrawingContext,
                                                D2D1_POINT_2F baselineOrigin,
                                                DWRITE_MEASURING_MODE measuringMode,
                                                _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription) noexcept;

        std::optional<D2D1_RECT_F> _clipRect;
    };
}
