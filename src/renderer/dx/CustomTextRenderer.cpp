// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CustomTextRenderer.h"

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
    IUnknown* /*clientDrawingEffect*/)
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

    // Draw the background
    D2D1_RECT_F rect;
    rect.top = origin.y;
    rect.bottom = rect.top + drawingContext->cellSize.height;
    rect.left = origin.x;
    rect.right = rect.left;
    const auto advancesSpan = gsl::make_span(glyphRun->glyphAdvances, glyphRun->glyphCount);

    rect.right = std::accumulate(advancesSpan.cbegin(), advancesSpan.cend(), rect.right);

    d2dContext->FillRectangle(rect, drawingContext->backgroundBrush);

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
                                                drawingContext->foregroundBrush));
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
                                                        layerBrush));
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
                                            drawingContext->foregroundBrush));
    }
    return S_OK;
}
#pragma endregion

[[nodiscard]] HRESULT CustomTextRenderer::_DrawBasicGlyphRun(DrawingContext* clientDrawingContext,
                                                             D2D1_POINT_2F baselineOrigin,
                                                             DWRITE_MEASURING_MODE measuringMode,
                                                             _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                             _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
                                                             ID2D1Brush* brush)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, glyphRun);
    RETURN_HR_IF_NULL(E_INVALIDARG, brush);

    ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;
    RETURN_IF_FAILED(clientDrawingContext->renderTarget->QueryInterface(d2dContext.GetAddressOf()));

    // Using the context is the easiest/default way of drawing.
    d2dContext->DrawGlyphRun(baselineOrigin, glyphRun, glyphRunDescription, brush, measuringMode);

    // However, we could probably add options here and switch out to one of these other drawing methods (making it
    // conditional based on the IUnknown* clientDrawingEffect or on some other switches and try these out instead:

    //_DrawBasicGlyphRunManually(clientDrawingContext, baselineOrigin, measuringMode, glyphRun, glyphRunDescription);
    //_DrawGlowGlyphRun(clientDrawingContext, baselineOrigin, measuringMode, glyphRun, glyphRunDescription);

    return S_OK;
}

[[nodiscard]] HRESULT CustomTextRenderer::_DrawBasicGlyphRunManually(DrawingContext* clientDrawingContext,
                                                                     D2D1_POINT_2F baselineOrigin,
                                                                     DWRITE_MEASURING_MODE /*measuringMode*/,
                                                                     _In_ const DWRITE_GLYPH_RUN* glyphRun,
                                                                     _In_opt_ const DWRITE_GLYPH_RUN_DESCRIPTION* /*glyphRunDescription*/) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, clientDrawingContext);
    RETURN_HR_IF_NULL(E_INVALIDARG, glyphRun);

    // This is regular text but manually
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

    clientDrawingContext->renderTarget->FillGeometry(transformedGeometry.Get(), clientDrawingContext->foregroundBrush);

    return S_OK;
}

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
