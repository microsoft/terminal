// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CustomTextRenderer.h"

#include "../../inc/DefaultSettings.h"

#include <wrl.h>
#include <wrl/client.h>
#include <VersionHelpers.h>

using namespace Microsoft::Console::Render;

#pragma region IDWritePixelSnapping methods
// Routine Description:
// - Implementation of IDWritePixelSnapping::IsPixelSnappingDisabled
// - Determines if we're allowed to snap text to pixels for this particular drawing context
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - isDisabled - TRUE if we do not snap to nearest pixels. FALSE otherwise.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::IsPixelSnappingDisabled(void* /*clientDrawingContext*/,
                                                                  _Out_ BOOL* isDisabled) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, isDisabled);

    *isDisabled = false;
    return S_OK;
}

// Routine Description:
// - Implementation of IDWritePixelSnapping::GetPixelsPerDip
// - Retrieves the number of real monitor pixels to use per device-independent-pixel (DIP)
// - DIPs are used by DirectX all the way until the final drawing surface so things are only
//   scaled at the very end and the complexity can be abstracted.
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - pixelsPerDip - The number of pixels per DIP. 96 is standard DPI.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::GetPixelsPerDip(void* clientDrawingContext,
                                                          _Out_ FLOAT* pixelsPerDip) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pixelsPerDip);

    const DrawingContext* drawingContext = static_cast<DrawingContext*>(clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, drawingContext);

    float dpiX, dpiY;
    drawingContext->renderTarget->GetDpi(&dpiX, &dpiY);
    *pixelsPerDip = dpiX / USER_DEFAULT_SCREEN_DPI;
    return S_OK;
}

// Routine Description:
// - Implementation of IDWritePixelSnapping::GetCurrentTransform
// - Retrieves the the matrix transform to be used while laying pixels onto the
//   drawing context
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - transform - The matrix transform to use to adapt DIP representations into real monitor coordinates.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::GetCurrentTransform(void* clientDrawingContext,
                                                              DWRITE_MATRIX* transform) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, transform);

    const DrawingContext* drawingContext = static_cast<DrawingContext*>(clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, drawingContext);

    // Retrieve as D2D1 matrix then copy into DWRITE matrix.
    D2D1_MATRIX_3X2_F d2d1Matrix{ 0 };
    drawingContext->renderTarget->GetTransform(&d2d1Matrix);

    transform->dx = d2d1Matrix.dx;
    transform->dy = d2d1Matrix.dy;
    transform->m11 = d2d1Matrix.m11;
    transform->m12 = d2d1Matrix.m12;
    transform->m21 = d2d1Matrix.m21;
    transform->m22 = d2d1Matrix.m22;

    return S_OK;
}
#pragma endregion

#pragma region IDWriteTextRenderer methods
// Routine Description:
// - Implementation of IDWriteTextRenderer::DrawUnderline
// - Directs us to draw an underline on the given context at the given position.
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - baselineOriginX - The text baseline position's X coordinate
// - baselineOriginY - The text baseline position's Y coordinate
//                   - The baseline is generally not the top nor the bottom of the "cell" that
//                     text is drawn into. It's usually somewhere "in the middle" and depends on the
//                     font and the glyphs. It can be calculated during layout and analysis in respect
//                     to the given font and glyphs.
// - underline - The properties of the underline that we should use for drawing
// - clientDrawingEffect - any special effect to pass along for rendering
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::DrawUnderline(void* clientDrawingContext,
                                                        FLOAT baselineOriginX,
                                                        FLOAT baselineOriginY,
                                                        _In_ const DWRITE_UNDERLINE* underline,
                                                        IUnknown* clientDrawingEffect) noexcept
{
    return _FillRectangle(clientDrawingContext,
                          clientDrawingEffect,
                          baselineOriginX,
                          baselineOriginY + underline->offset,
                          underline->width,
                          underline->thickness,
                          underline->readingDirection,
                          underline->flowDirection);
}

// Routine Description:
// - Implementation of IDWriteTextRenderer::DrawStrikethrough
// - Directs us to draw a strikethrough on the given context at the given position.
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - baselineOriginX - The text baseline position's X coordinate
// - baselineOriginY - The text baseline position's Y coordinate
//                   - The baseline is generally not the top nor the bottom of the "cell" that
//                     text is drawn into. It's usually somewhere "in the middle" and depends on the
//                     font and the glyphs. It can be calculated during layout and analysis in respect
//                     to the given font and glyphs.
// - strikethrough - The properties of the strikethrough that we should use for drawing
// - clientDrawingEffect - any special effect to pass along for rendering
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::DrawStrikethrough(void* clientDrawingContext,
                                                            FLOAT baselineOriginX,
                                                            FLOAT baselineOriginY,
                                                            _In_ const DWRITE_STRIKETHROUGH* strikethrough,
                                                            IUnknown* clientDrawingEffect) noexcept
{
    return _FillRectangle(clientDrawingContext,
                          clientDrawingEffect,
                          baselineOriginX,
                          baselineOriginY + strikethrough->offset,
                          strikethrough->width,
                          strikethrough->thickness,
                          strikethrough->readingDirection,
                          strikethrough->flowDirection);
}

// Routine Description:
// - Helper method to draw a line through our text.
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - clientDrawingEffect - any special effect passed along for rendering
// - x - The left coordinate of the rectangle
// - y - The top coordinate of the rectangle
// - width - The width of the rectangle (from X to the right)
// - height - The height of the rectangle (from Y down)
// - readingDirection - textual reading information that could affect the rectangle
// - flowDirection - textual flow information that could affect the rectangle
// Return Value:
// - S_OK
[[nodiscard]] HRESULT CustomTextRenderer::_FillRectangle(void* clientDrawingContext,
                                                         IUnknown* clientDrawingEffect,
                                                         float x,
                                                         float y,
                                                         float width,
                                                         float thickness,
                                                         DWRITE_READING_DIRECTION /*readingDirection*/,
                                                         DWRITE_FLOW_DIRECTION /*flowDirection*/) noexcept
{
    DrawingContext* drawingContext = static_cast<DrawingContext*>(clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, drawingContext);

    // Get brush
    ID2D1Brush* brush = drawingContext->foregroundBrush;

    if (clientDrawingEffect != nullptr)
    {
        brush = static_cast<ID2D1Brush*>(clientDrawingEffect);
    }

    const D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + thickness);
    drawingContext->renderTarget->FillRectangle(&rect, brush);

    return S_OK;
}

// Routine Description:
// - Implementation of IDWriteTextRenderer::DrawInlineObject
// - Passes drawing control from the outer layout down into the context of an embedded object
//   which can have its own drawing layout and renderer properties at a given position
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - originX - The left coordinate of the draw position
// - originY - The top coordinate of the draw position
// - inlineObject - The object to draw at the position
// - isSideways - Should be drawn vertically instead of horizontally
// - isRightToLeft - Should be drawn RTL (or bottom to top) instead of the default way
// - clientDrawingEffect - any special effect passed along for rendering
// Return Value:
// - S_OK or appropriate error from the delegated inline object's draw call
[[nodiscard]] HRESULT CustomTextRenderer::DrawInlineObject(void* clientDrawingContext,
                                                           FLOAT originX,
                                                           FLOAT originY,
                                                           IDWriteInlineObject* inlineObject,
                                                           BOOL isSideways,
                                                           BOOL isRightToLeft,
                                                           IUnknown* clientDrawingEffect) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, inlineObject);

    return inlineObject->Draw(clientDrawingContext,
                              this,
                              originX,
                              originY,
                              isSideways,
                              isRightToLeft,
                              clientDrawingEffect);
}

// Function Description:
// - Attempt to draw the cursor. If the cursor isn't visible or on, this
//   function will do nothing. If the cursor isn't within the bounds of the
//   current run of text, then this function will do nothing.
// - This function will get called twice during a run, once before the text is
//   drawn (underneath the text), and again after the text is drawn (above the
//   text). Depending on if the cursor wants to be drawn above or below the
//   text, this function will do nothing for the first/second pass
//   (respectively).
// Arguments:
// - d2dContext - Pointer to the current D2D drawing context
// - textRunBounds - The bounds of the current run of text.
// - drawingContext - Pointer to structure of information required to draw
// - firstPass - true if we're being called before the text is drawn, false afterwards.
// Return Value:
// - S_FALSE if we did nothing, S_OK if we successfully painted, otherwise an appropriate HRESULT
[[nodiscard]] HRESULT _drawCursor(gsl::not_null<ID2D1DeviceContext*> d2dContext,
                                  D2D1_RECT_F textRunBounds,
                                  const DrawingContext& drawingContext,
                                  const bool firstPass)
try
{
    if (!drawingContext.cursorInfo.has_value())
    {
        return S_FALSE;
    }

    const auto& options = drawingContext.cursorInfo.value();

    // if the cursor is off, do nothing - it should not be visible.
    if (!options.isOn)
    {
        return S_FALSE;
    }

    const bool fInvert = !options.fUseColor;
    // The normal, colored FullBox and legacy cursors are drawn in the first pass
    // so they go behind the text.
    // Inverted cursors are drawn in two passes.
    // All other cursors are drawn in the second pass only.
    if (!fInvert)
    {
        if (firstPass != (options.cursorType == CursorType::FullBox))
        {
            return S_FALSE;
        }
    }

    // TODO GH#6338: Add support for `"cursorTextColor": null` for letting the
    // cursor draw on top again.

    // **MATH** PHASE
    const til::size glyphSize{ til::math::flooring,
                               drawingContext.cellSize.width,
                               drawingContext.cellSize.height };

    // Create rectangular block representing where the cursor can fill.
    D2D1_RECT_F rect = til::rectangle{ til::point{ options.coordCursor } }.scale_up(glyphSize);

    // If we're double-width, make it one extra glyph wider
    if (options.fIsDoubleWidth)
    {
        rect.right += glyphSize.width();
    }

    // If the cursor isn't within the bounds of this current run of text, do nothing.
    if (rect.top > textRunBounds.bottom ||
        rect.bottom <= textRunBounds.top ||
        rect.left > textRunBounds.right ||
        rect.right <= textRunBounds.left)
    {
        return S_FALSE;
    }

    CursorPaintType paintType = CursorPaintType::Fill;
    switch (options.cursorType)
    {
    case CursorType::Legacy:
    {
        // Enforce min/max cursor height
        ULONG ulHeight = std::clamp(options.ulCursorHeightPercent, MinCursorHeightPercent, MaxCursorHeightPercent);
        ulHeight = (glyphSize.height<ULONG>() * ulHeight) / 100;
        ulHeight = std::max(ulHeight, MinCursorHeightPixels); // No smaller than 1px

        rect.top = rect.bottom - ulHeight;
        break;
    }
    case CursorType::VerticalBar:
    {
        // It can't be wider than one cell or we'll have problems in invalidation, so restrict here.
        // It's either the left + the proposed width from the ease of access setting, or
        // it's the right edge of the block cursor as a maximum.
        rect.right = std::min(rect.right, rect.left + options.cursorPixelWidth);
        break;
    }
    case CursorType::Underscore:
    {
        rect.top = rect.bottom - 1;
        break;
    }
    case CursorType::DoubleUnderscore:
    {
        // Use rect for lower line.
        rect.top = rect.bottom - 1;
        break;
    }
    case CursorType::EmptyBox:
    {
        paintType = CursorPaintType::Outline;
        break;
    }
    case CursorType::FullBox:
    {
        break;
    }
    default:
        return E_NOTIMPL;
    }

    // **DRAW** PHASE
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<ID2D1Image> originalTarget;
    Microsoft::WRL::ComPtr<ID2D1CommandList> commandList;
    if (!fInvert)
    {
        // Make sure to make the cursor opaque
        RETURN_IF_FAILED(d2dContext->CreateSolidColorBrush(til::color{ OPACITY_OPAQUE | options.cursorColor },
                                                           &brush));
    }
    else
    {
        // CURSOR INVERSION
        // We're trying to invert the cursor and the character underneath it without redrawing the text (as
        // doing so would break up the run if it were part of a ligature). To do that, we're going to try
        // to invert the content of the screen where the cursor would have been.
        //
        // This renderer, however, supports transparency. In fact, in its default configuration it will not
        // have a background at all (it delegates background handling to somebody else.) You can't invert what
        // isn't there.
        //
        // To properly invert the cursor in such a configuration, then, we have to play some tricks. Examples
        // are given below for two cursor types, but this applies to all of them.
        //
        // First, we'll draw a "backplate" in the user's requested background color (with the alpha channel
        // set to 0xFF). (firstPass == true)
        //
        // EMPTY BOX  FILLED BOX
        // =====      =====
        // =   =      =====
        // =   =      =====
        // =   =      =====
        // =====      =====
        //
        // Then, outside of _drawCursor, the glyph is drawn:
        //
        // EMPTY BOX  FILLED BOX
        // ==A==      ==A==
        // =A A=      =A=A=
        // AAAAA      AAAAA
        // A   A      A===A
        // A===A      A===A
        //
        // Last, we'll draw the cursor again in all white and use that as the *mask* for inverting the already-
        // drawn pixels. (firstPass == false) (# = mask, a = inverted A)
        //
        // EMPTY BOX  FILLED BOX
        // ##a##      ##a##
        // #A A#      #a#a#
        // aAAAa      aaaaa
        // a   a      a###a
        // a###a      a###a
        if (firstPass)
        {
            // Draw a backplate behind the cursor in the *background* color so that we can invert it later.
            // We're going to draw the exact same color as the background behind the cursor
            const til::color color{ drawingContext.backgroundBrush->GetColor() };
            RETURN_IF_FAILED(d2dContext->CreateSolidColorBrush(color.with_alpha(255),
                                                               &brush));
        }
        else
        {
            // When we're drawing an inverted cursor on the second pass (foreground), we want to draw it into a
            // command list, which we will then draw down with MASK_INVERT. We'll draw it in white,
            // which will ensure that every component is masked.
            RETURN_IF_FAILED(d2dContext->CreateCommandList(&commandList));
            d2dContext->GetTarget(&originalTarget);
            d2dContext->SetTarget(commandList.Get());
            RETURN_IF_FAILED(d2dContext->CreateSolidColorBrush(COLOR_WHITE, &brush));
        }
    }

    switch (paintType)
    {
    case CursorPaintType::Fill:
    {
        d2dContext->FillRectangle(rect, brush.Get());
        break;
    }
    case CursorPaintType::Outline:
    {
        // DrawRectangle in straddles physical pixels in an attempt to draw a line
        // between them. To avoid this, bump the rectangle around by half the stroke width.
        rect.top += 0.5f;
        rect.left += 0.5f;
        rect.bottom -= 0.5f;
        rect.right -= 0.5f;

        d2dContext->DrawRectangle(rect, brush.Get());
        break;
    }
    default:
        return E_NOTIMPL;
    }

    if (options.cursorType == CursorType::DoubleUnderscore)
    {
        // Draw upper line directly.
        D2D1_RECT_F upperLine = rect;
        upperLine.top -= 2;
        upperLine.bottom -= 2;
        d2dContext->FillRectangle(upperLine, brush.Get());
    }

    if (commandList)
    {
        // We drew the entire cursor in a command list
        // so now we draw that command list using MASK_INVERT over the existing image
        RETURN_IF_FAILED(commandList->Close());
        d2dContext->SetTarget(originalTarget.Get());
        d2dContext->DrawImage(commandList.Get(), D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_MASK_INVERT);
    }

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Implementation of IDWriteTextRenderer::DrawInlineObject
// - Passes drawing control from the outer layout down into the context of an embedded object
//   which can have its own drawing layout and renderer properties at a given position
// Arguments:
// - clientDrawingContext - Pointer to structure of information required to draw
// - baselineOriginX - The text baseline position's X coordinate
// - baselineOriginY - The text baseline position's Y coordinate
//                   - The baseline is generally not the top nor the bottom of the "cell" that
//                     text is drawn into. It's usually somewhere "in the middle" and depends on the
//                     font and the glyphs. It can be calculated during layout and analysis in respect
//                     to the given font and glyphs.
// - measuringMode - The mode to measure glyphs in the DirectWrite context
// - glyphRun - Information on the glyphs
// - glyphRunDescription - Further metadata about the glyphs used while drawing
// - clientDrawingEffect - any special effect passed along for rendering
// Return Value:
// - S_OK, GSL/WIL/STL error, or appropriate DirectX/Direct2D/DirectWrite based error while drawing.
[[nodiscard]] HRESULT CustomTextRenderer::DrawGlyphRun(
    void* clientDrawingContext,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    DWRITE_MEASURING_MODE measuringMode,
    const DWRITE_GLYPH_RUN* glyphRun,
    const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
    IUnknown* clientDrawingEffect)
{
    // Color glyph rendering sourced from https://github.com/Microsoft/Windows-universal-samples/tree/master/Samples/DWriteColorGlyph

    DrawingContext* drawingContext = static_cast<DrawingContext*>(clientDrawingContext);

    // Since we've delegated the drawing of the background of the text into this function, the origin passed in isn't actually the baseline.
    // It's the top left corner. Save that off first.
    const D2D1_POINT_2F origin = D2D1::Point2F(baselineOriginX, baselineOriginY);

    // Then make a copy for the baseline origin (which is part way down the left side of the text, not the top or bottom).
    // We'll use this baseline Origin for drawing the actual text.
    const D2D1_POINT_2F baselineOrigin{ origin.x, origin.y + drawingContext->spacing.baseline };

    ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;
    RETURN_IF_FAILED(drawingContext->renderTarget->QueryInterface(d2dContext.GetAddressOf()));

    // Determine clip rectangle
    D2D1_RECT_F clipRect;
    clipRect.top = origin.y;
    clipRect.bottom = clipRect.top + drawingContext->cellSize.height;
    clipRect.left = 0;
    clipRect.right = drawingContext->targetSize.width;

    // If we already have a clip rectangle, check if it different than the previous one.
    if (_clipRect.has_value())
    {
        const auto storedVal = _clipRect.value();
        // If it is different, pop off the old one and push the new one on.
        if (storedVal.top != clipRect.top || storedVal.bottom != clipRect.bottom ||
            storedVal.left != clipRect.left || storedVal.right != clipRect.right)
        {
            d2dContext->PopAxisAlignedClip();

            // Clip all drawing in this glyph run to where we expect.
            // We need the AntialiasMode here to be Aliased to ensure
            //  that background boxes line up with each other and don't leave behind
            //  stray colors.
            // See GH#3626 for more details.
            d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
            _clipRect = clipRect;
        }
    }
    // If we have no clip rectangle, it's easy. Push it on and go.
    else
    {
        // See above for aliased flag explanation.
        d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
        _clipRect = clipRect;
    }

    // Draw the background
    // The rectangle needs to be deduced based on the origin and the BidiDirection
    const auto advancesSpan = gsl::make_span(glyphRun->glyphAdvances, glyphRun->glyphCount);
    const auto totalSpan = std::accumulate(advancesSpan.begin(), advancesSpan.end(), 0.0f);

    D2D1_RECT_F rect;
    rect.top = origin.y;
    rect.bottom = rect.top + drawingContext->cellSize.height;
    rect.left = origin.x;
    // Check for RTL, if it is, move rect.left to the left from the baseline
    if (WI_IsFlagSet(glyphRun->bidiLevel, 1))
    {
        rect.left -= totalSpan;
    }
    rect.right = rect.left + totalSpan;

    d2dContext->FillRectangle(rect, drawingContext->backgroundBrush);

    RETURN_IF_FAILED(_drawCursor(d2dContext.Get(), rect, *drawingContext, true));

    // GH#5098: If we're rendering with cleartype text, we need to always render
    // onto an opaque background. If our background _isn't_ opaque, then we need
    // to use grayscale AA for this run of text.
    //
    // We can force grayscale AA for just this run of text by pushing a new
    // layer onto the d2d context. We'll only need to do this for cleartype
    // text, when our eventual background isn't actually opaque. See
    // DxEngine::PaintBufferLine and DxEngine::UpdateDrawingBrushes for more
    // details.
    //
    // DANGER: Layers slow us down. Only do this in the specific case where
    // someone has chosen the slower ClearType antialiasing (versus the faster
    // grayscale antialiasing).

    // First, create the scope_exit to pop the layer. If we don't need the
    // layer, we'll just gracefully release it.
    auto popLayer = wil::scope_exit([&d2dContext]() noexcept {
        d2dContext->PopLayer();
    });

    if (drawingContext->forceGrayscaleAA)
    {
        // Mysteriously, D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE actually
        // gets us the behavior we want, which is grayscale.
        d2dContext->PushLayer(D2D1::LayerParameters(rect,
                                                    nullptr,
                                                    D2D1_ANTIALIAS_MODE_ALIASED,
                                                    D2D1::IdentityMatrix(),
                                                    1.0,
                                                    nullptr,
                                                    D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE),
                              nullptr);
    }
    else
    {
        popLayer.release();
    }
    // Now go onto drawing the text.

    // First check if we want a color font and try to extract color emoji first.
    // Color emoji are only available on Windows 10+
    static const bool s_isWindows10OrGreater = IsWindows10OrGreater();
    if (WI_IsFlagSet(drawingContext->options, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT) && s_isWindows10OrGreater)
    {
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext4> d2dContext4;
        RETURN_IF_FAILED(d2dContext.As(&d2dContext4));

        ::Microsoft::WRL::ComPtr<IDWriteFactory4> dwriteFactory4;
        RETURN_IF_FAILED(drawingContext->dwriteFactory->QueryInterface(dwriteFactory4.GetAddressOf()));

        // The list of glyph image formats this renderer is prepared to support.
        const DWRITE_GLYPH_IMAGE_FORMATS supportedFormats =
            DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
            DWRITE_GLYPH_IMAGE_FORMATS_CFF |
            DWRITE_GLYPH_IMAGE_FORMATS_COLR |
            DWRITE_GLYPH_IMAGE_FORMATS_SVG |
            DWRITE_GLYPH_IMAGE_FORMATS_PNG |
            DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
            DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
            DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;

        // Determine whether there are any color glyph runs within glyphRun. If
        // there are, glyphRunEnumerator can be used to iterate through them.
        ::Microsoft::WRL::ComPtr<IDWriteColorGlyphRunEnumerator1> glyphRunEnumerator;
        const HRESULT hr = dwriteFactory4->TranslateColorGlyphRun(baselineOrigin,
                                                                  glyphRun,
                                                                  glyphRunDescription,
                                                                  supportedFormats,
                                                                  measuringMode,
                                                                  nullptr,
                                                                  0,
                                                                  &glyphRunEnumerator);

        // If the analysis found no color glyphs in the run, just draw normally.
        if (hr == DWRITE_E_NOCOLOR)
        {
            RETURN_IF_FAILED(_DrawBasicGlyphRun(drawingContext,
                                                baselineOrigin,
                                                measuringMode,
                                                glyphRun,
                                                glyphRunDescription,
                                                drawingContext->foregroundBrush,
                                                clientDrawingEffect));
        }
        else
        {
            RETURN_IF_FAILED(hr);

            ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tempBrush;

            // Complex case: the run has one or more color runs within it. Iterate
            // over the sub-runs and draw them, depending on their format.
            for (;;)
            {
                BOOL haveRun;
                RETURN_IF_FAILED(glyphRunEnumerator->MoveNext(&haveRun));
                if (!haveRun)
                    break;

                DWRITE_COLOR_GLYPH_RUN1 const* colorRun;
                RETURN_IF_FAILED(glyphRunEnumerator->GetCurrentRun(&colorRun));

                const D2D1_POINT_2F currentBaselineOrigin = D2D1::Point2F(colorRun->baselineOriginX, colorRun->baselineOriginY);

                switch (colorRun->glyphImageFormat)
                {
                case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
                case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
                case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
                case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
                {
                    // This run is bitmap glyphs. Use Direct2D to draw them.
                    d2dContext4->DrawColorBitmapGlyphRun(colorRun->glyphImageFormat,
                                                         currentBaselineOrigin,
                                                         &colorRun->glyphRun,
                                                         measuringMode);
                }
                break;

                case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
                {
                    // This run is SVG glyphs. Use Direct2D to draw them.
                    d2dContext4->DrawSvgGlyphRun(currentBaselineOrigin,
                                                 &colorRun->glyphRun,
                                                 drawingContext->foregroundBrush,
                                                 nullptr, // svgGlyphStyle
                                                 0, // colorPaletteIndex
                                                 measuringMode);
                }
                break;

                case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
                case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
                case DWRITE_GLYPH_IMAGE_FORMATS_COLR:
                default:
                {
                    // This run is solid-color outlines, either from non-color
                    // glyphs or from COLR glyph layers. Use Direct2D to draw them.

                    ID2D1Brush* layerBrush{ nullptr };
                    // The rule is "if 0xffff, use current brush." See:
                    // https://docs.microsoft.com/en-us/windows/desktop/api/dwrite_2/ns-dwrite_2-dwrite_color_glyph_run
                    if (colorRun->paletteIndex == 0xFFFF)
                    {
                        // This run uses the current text color.
                        layerBrush = drawingContext->foregroundBrush;
                    }
                    else
                    {
                        if (!tempBrush)
                        {
                            RETURN_IF_FAILED(d2dContext4->CreateSolidColorBrush(colorRun->runColor, &tempBrush));
                        }
                        else
                        {
                            // This run specifies its own color.
                            tempBrush->SetColor(colorRun->runColor);
                        }
                        layerBrush = tempBrush.Get();
                    }

                    // Draw the run with the selected color.
                    RETURN_IF_FAILED(_DrawBasicGlyphRun(drawingContext,
                                                        currentBaselineOrigin,
                                                        measuringMode,
                                                        &colorRun->glyphRun,
                                                        colorRun->glyphRunDescription,
                                                        layerBrush,
                                                        clientDrawingEffect));
                }
                break;
                }
            }
        }
    }
    else
    {
        // Simple case: the run has no color glyphs. Draw the main glyph run
        // using the current text color.
        RETURN_IF_FAILED(_DrawBasicGlyphRun(drawingContext,
                                            baselineOrigin,
                                            measuringMode,
                                            glyphRun,
                                            glyphRunDescription,
                                            drawingContext->foregroundBrush,
                                            clientDrawingEffect));
    }

    RETURN_IF_FAILED(_drawCursor(d2dContext.Get(), rect, *drawingContext, false));

    return S_OK;
}
#pragma endregion

[[nodiscard]] HRESULT CustomTextRenderer::EndClip(void* clientDrawingContext) noexcept
try
{
    DrawingContext* drawingContext = static_cast<DrawingContext*>(clientDrawingContext);
    RETURN_HR_IF(E_INVALIDARG, !drawingContext);

    if (_clipRect.has_value())
    {
        drawingContext->renderTarget->PopAxisAlignedClip();
        _clipRect = std::nullopt;
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT CustomTextRenderer::_DrawBasicGlyphRun(DrawingContext* clientDrawingContext,
                                                             D2D1_POINT_2F baselineOrigin,
                                                             DWRITE_MEASURING_MODE measuringMode,
                                                             _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                             _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                                             ID2D1Brush* brush,
                                                             _In_opt_ IUnknown* clientDrawingEffect)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, glyphRun);
    RETURN_HR_IF_NULL(E_INVALIDARG, brush);

    ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;
    RETURN_IF_FAILED(clientDrawingContext->renderTarget->QueryInterface(d2dContext.GetAddressOf()));

    // If a special drawing effect was specified, see if we know how to deal with it.
    if (clientDrawingEffect)
    {
        ::Microsoft::WRL::ComPtr<IBoxDrawingEffect> boxEffect;
        if (SUCCEEDED(clientDrawingEffect->QueryInterface<IBoxDrawingEffect>(&boxEffect)))
        {
            return _DrawBoxRunManually(clientDrawingContext, baselineOrigin, measuringMode, glyphRun, glyphRunDescription, boxEffect.Get());
        }

        //_DrawBasicGlyphRunManually(clientDrawingContext, baselineOrigin, measuringMode, glyphRun, glyphRunDescription);
        //_DrawGlowGlyphRun(clientDrawingContext, baselineOrigin, measuringMode, glyphRun, glyphRunDescription);
    }

    // If we get down here, there either was no special effect or we don't know what to do with it. Use the standard GlyphRun drawing.

    // Using the context is the easiest/default way of drawing.
    d2dContext->DrawGlyphRun(baselineOrigin, glyphRun, glyphRunDescription, brush, measuringMode);

    return S_OK;
}

[[nodiscard]] HRESULT CustomTextRenderer::_DrawBoxRunManually(DrawingContext* clientDrawingContext,
                                                              D2D1_POINT_2F baselineOrigin,
                                                              DWRITE_MEASURING_MODE /*measuringMode*/,
                                                              _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                              _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* /*glyphRunDescription*/,
                                                              _In_ IBoxDrawingEffect* clientDrawingEffect) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, glyphRun);
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingEffect);

    ::Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory;
    clientDrawingContext->renderTarget->GetFactory(d2dFactory.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1PathGeometry> pathGeometry;
    d2dFactory->CreatePathGeometry(pathGeometry.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1GeometrySink> geometrySink;
    pathGeometry->Open(geometrySink.GetAddressOf());

    glyphRun->fontFace->GetGlyphRunOutline(
        glyphRun->fontEmSize,
        glyphRun->glyphIndices,
        glyphRun->glyphAdvances,
        glyphRun->glyphOffsets,
        glyphRun->glyphCount,
        glyphRun->isSideways,
        glyphRun->bidiLevel % 2,
        geometrySink.Get());

    geometrySink->Close();

    // Can be used to see the dimensions of what is written.
    /*D2D1_RECT_F bounds;
    pathGeometry->GetBounds(D2D1::IdentityMatrix(), &bounds);*/
    // The bounds here are going to be centered around the baseline of the font.
    // That is, the DWRITE_GLYPH_METRICS property for this glyph's baseline is going
    // to be at the 0 point in the Y direction when we receive the geometry.
    // The ascent will go up negative from Y=0 and the descent will go down positive from Y=0.
    // As for the horizontal direction, I didn't study this in depth, but it appears to always be
    // positive X with both the left and right edges being positive and away from X=0.
    // For one particular instance, we might ask for the geometry for a U+2588 box and see the bounds as:
    //
    //                                 Top=
    //                               -20.315
    //                             -----------
    //                             |         |
    //                             |         |
    //                       Left= |         | Right=
    //                     13.859  |         | 26.135
    //                             |         |
    //    Origin --> X             |         |
    //    (0,0)                    |         |
    //                             -----------
    //                               Bottom=
    //                               5.955

    // Dig out the box drawing effect parameters.
    BoxScale scale;
    RETURN_IF_FAILED(clientDrawingEffect->GetScale(&scale));

    // The scale transform will inflate the entire geometry first.
    // We want to do this before it moves out of its original location as generally our
    // algorithms for fitting cells will blow up the glyph to the size it needs to be first and then
    // nudge it into place with the translations.
    const auto scaleTransform = D2D1::Matrix3x2F::Scale(scale.HorizontalScale, scale.VerticalScale);

    // Now shift it all the way to where the baseline says it should be.
    const auto baselineTransform = D2D1::Matrix3x2F::Translation(baselineOrigin.x, baselineOrigin.y);

    // Finally apply the little "nudge" that we may have been directed to align it better with the cell.
    const auto offsetTransform = D2D1::Matrix3x2F::Translation(scale.HorizontalTranslation, scale.VerticalTranslation);

    // The order is important here. Scale it first, then slide it into place.
    const auto matrixTransformation = scaleTransform * baselineTransform * offsetTransform;

    ::Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> transformedGeometry;
    d2dFactory->CreateTransformedGeometry(pathGeometry.Get(),
                                          &matrixTransformation,
                                          transformedGeometry.GetAddressOf());

    // Can be used to see the dimensions after translation.
    /*D2D1_RECT_F boundsAfter;
    transformedGeometry->GetBounds(D2D1::IdentityMatrix(), &boundsAfter);*/
    // Compare this to the original bounds above to see what the matrix did.
    // To make it useful, first visualize for yourself the pixel dimensions of the cell
    // based on the baselineOrigin and the exact integer cell width and heights that we're storing.
    // You'll also probably need the full-pixel ascent and descent because the point we're given
    // is the baseline, not the top left corner of the cell as we're used to.
    // Most of these metrics can be found in the initial font creation routines or in
    // the line spacing applied to the text format (member variables on the renderer).
    // baselineOrigin = (0, 567)
    // fullPixelAscent = 39
    // fullPixelDescent = 9
    // cell dimensions = 26 x 48 (notice 48 height is 39 + 9 or ascent + descent)
    // This means that our cell should be the rectangle
    //
    //             T=528
    //           |-------|
    //       L=0 |       |
    //           |       |
    // Baseline->x       |
    //  Origin   |       | R=26
    //           |-------|
    //             B=576
    //
    // And we'll want to check that the bounds after transform will fit the glyph nicely inside
    // this box.
    // If not? We didn't do the scaling or translation correctly. Oops.

    // Fill in the geometry. Don't outline, it can leave stuff outside the area we expect.
    clientDrawingContext->renderTarget->FillGeometry(transformedGeometry.Get(), clientDrawingContext->foregroundBrush);

    return S_OK;
}
CATCH_RETURN();

[[nodiscard]] HRESULT CustomTextRenderer::_DrawGlowGlyphRun(DrawingContext* clientDrawingContext,
                                                            D2D1_POINT_2F baselineOrigin,
                                                            DWRITE_MEASURING_MODE /*measuringMode*/,
                                                            _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                            _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* /*glyphRunDescription*/) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, glyphRun);

    // This is glow text manually
    ::Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory;
    clientDrawingContext->renderTarget->GetFactory(d2dFactory.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1PathGeometry> pathGeometry;
    d2dFactory->CreatePathGeometry(pathGeometry.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1GeometrySink> geometrySink;
    pathGeometry->Open(geometrySink.GetAddressOf());

    glyphRun->fontFace->GetGlyphRunOutline(
        glyphRun->fontEmSize,
        glyphRun->glyphIndices,
        glyphRun->glyphAdvances,
        glyphRun->glyphOffsets,
        glyphRun->glyphCount,
        glyphRun->isSideways,
        glyphRun->bidiLevel % 2,
        geometrySink.Get());

    geometrySink->Close();

    D2D1::Matrix3x2F const matrixAlign = D2D1::Matrix3x2F::Translation(baselineOrigin.x, baselineOrigin.y);

    ::Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> transformedGeometry;
    d2dFactory->CreateTransformedGeometry(pathGeometry.Get(),
                                          &matrixAlign,
                                          transformedGeometry.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1TransformedGeometry> alignedGeometry;
    d2dFactory->CreateTransformedGeometry(pathGeometry.Get(),
                                          &matrixAlign,
                                          alignedGeometry.GetAddressOf());

    ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> outlineBrush;

    clientDrawingContext->renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), brush.GetAddressOf());
    clientDrawingContext->renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red, 1.0f), outlineBrush.GetAddressOf());

    clientDrawingContext->renderTarget->DrawGeometry(transformedGeometry.Get(), outlineBrush.Get(), 2.0f);

    clientDrawingContext->renderTarget->FillGeometry(alignedGeometry.Get(), brush.Get());

    return S_OK;
}
