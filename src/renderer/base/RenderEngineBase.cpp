// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderEngineBase.hpp"
#include "../../buffer/out/textBuffer.hpp"

#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

RenderEngineBase::RenderEngineBase() :
    _titleChanged(false),
    _lastFrameTitle(L"")
{
}

HRESULT RenderEngineBase::InvalidateTitle(const std::wstring_view proposedTitle) noexcept
{
    if (proposedTitle != _lastFrameTitle)
    {
        _titleChanged = true;
    }

    return S_OK;
}

// Method Description:
// - By default, no one should need continuous redraw. It ruins performance
//   in terms of CPU, memory, and battery life to just paint forever.
//   That's why we sleep when there's nothing to draw.
//   But if you REALLY WANT to do special effects... you need to keep painting.
[[nodiscard]] bool RenderEngineBase::RequiresContinuousRedraw() noexcept
{
    return false;
}

// Method Description:
// - Blocks until the engine is able to render without blocking.
void RenderEngineBase::WaitUntilCanRender() noexcept
{
    // do nothing by default
}

[[nodiscard]] std::optional<CursorOptions> RenderEngineBase::_GetCursorInfo(IRenderData* pData)
{
    if (pData->IsCursorVisible())
    {
        // Get cursor position in buffer
        COORD coordCursor = pData->GetCursorPosition();

        // GH#3166: Only draw the cursor if it's actually in the viewport. It
        // might be on the line that's in that partially visible row at the
        // bottom of the viewport, the space that's not quite a full line in
        // height. Since we don't draw that text, we shouldn't draw the cursor
        // there either.

        // The cursor is never rendered as double height, so we don't care about
        // the exact line rendition - only whether it's double width or not.
        const auto doubleWidth = pData->GetTextBuffer().IsDoubleWidthLine(coordCursor.Y);
        const auto lineRendition = doubleWidth ? LineRendition::DoubleWidth : LineRendition::SingleWidth;

        // We need to convert the screen coordinates of the viewport to an
        // equivalent range of buffer cells, taking line rendition into account.
        const auto view = ScreenToBufferLine(pData->GetViewport().ToInclusive(), lineRendition);

        // Note that we allow the X coordinate to be outside the left border by 1 position,
        // because the cursor could still be visible if the focused character is double width.
        const auto xInRange = coordCursor.X >= view.Left - 1 && coordCursor.X <= view.Right;
        const auto yInRange = coordCursor.Y >= view.Top && coordCursor.Y <= view.Bottom;
        if (xInRange && yInRange)
        {
            // Adjust cursor Y offset to viewport.
            // The viewport X offset is saved in the options and handled with a transform.
            coordCursor.Y -= view.Top;

            COLORREF cursorColor = pData->GetCursorColor();
            bool useColor = cursorColor != INVALID_COLOR;

            // Build up the cursor parameters including position, color, and drawing options
            CursorOptions options;
            options.coordCursor = coordCursor;
            options.viewportLeft = pData->GetViewport().Left();
            options.lineRendition = lineRendition;
            options.ulCursorHeightPercent = pData->GetCursorHeight();
            options.cursorPixelWidth = pData->GetCursorPixelWidth();
            options.fIsDoubleWidth = pData->IsCursorDoubleWidth();
            options.cursorType = pData->GetCursorStyle();
            options.fUseColor = useColor;
            options.cursorColor = cursorColor;
            options.isOn = pData->IsCursorOn();

            return { options };
        }
    }
    return std::nullopt;
}

void RenderEngineBase::_LoopDirtyLines(IRenderData* pData, std::function<void(BufferLineRenderData&)> action)
{
    // This is the subsection of the entire screen buffer that is currently being presented.
    // It can move left/right or top/bottom depending on how the viewport is scrolled
    // relative to the entire buffer.
    const auto view = pData->GetViewport();

    // This is effectively the number of cells on the visible screen that need to be redrawn.
    // The origin is always 0, 0 because it represents the screen itself, not the underlying buffer.
    gsl::span<const til::rectangle> dirtyAreas;
    LOG_IF_FAILED(GetDirtyArea(dirtyAreas));

    // Calling pData virtual functions is expensive. This won't change during painting.
    const auto globalInvert = pData->IsScreenReversed();
    const auto gridLineDrawingAllowed = pData->IsGridLineDrawingAllowed();

    for (const auto& dirtyRect : dirtyAreas)
    {
        auto dirty = Viewport::FromInclusive(dirtyRect);

        // Shift the origin of the dirty region to match the underlying buffer so we can
        // compare the two regions directly for intersection.
        dirty = Viewport::Offset(dirty, view.Origin());

        // The intersection between what is dirty on the screen (in need of repaint)
        // and what is supposed to be visible on the screen (the viewport) is what
        // we need to walk through line-by-line and repaint onto the screen.
        const auto redraw = Viewport::Intersect(dirty, view);

        // Shortcut: don't bother redrawing if the width is 0.
        if (redraw.Width() > 0)
        {
            // Retrieve the text buffer so we can read information out of it.
            const auto& buffer = pData->GetTextBuffer();

            // Now walk through each row of text that we need to redraw.
            for (auto row = redraw.Top(); row < redraw.BottomExclusive(); row++)
            {
                // Calculate the boundaries of a single line. This is from the left to right edge of the dirty
                // area in width and exactly 1 tall.
                const auto screenLine = SMALL_RECT{ redraw.Left(), row, redraw.RightInclusive(), row };

                // Convert the screen coordinates of the line to an equivalent
                // range of buffer cells, taking line rendition into account.
                const auto lineRendition = buffer.GetLineRendition(row);
                const auto bufferLine = Viewport::FromInclusive(ScreenToBufferLine(screenLine, lineRendition));

                // Find where on the screen we should place this line information. This requires us to re-map
                // the buffer-based origin of the line back onto the screen-based origin of the line.
                // For example, the screen might say we need to paint line 1 because it is dirty but the viewport
                // is actually looking at line 26 relative to the buffer. This means that we need line 27 out
                // of the backing buffer to fill in line 1 of the screen.
                const auto screenPosition = bufferLine.Origin() - COORD{ 0, view.Top() };

                // Calculate if two things are true:
                // 1. this row wrapped
                // 2. We're painting the last col of the row.
                // In that case, set lineWrapped=true for the _PaintBufferOutputHelper call.
                const auto lineWrapped = (buffer.GetRowByOffset(bufferLine.Origin().Y).WasWrapForced()) &&
                                         (bufferLine.RightExclusive() == buffer.GetSize().Width());

                BufferLineRenderData renderData{
                    pData,
                    buffer,
                    bufferLine.Origin(),
                    bufferLine,
                    screenPosition,
                    view,
                    lineRendition,
                    true,
                    lineWrapped,
                    globalInvert,
                    gridLineDrawingAllowed
                };
                action(renderData);
            }
        }
    }
}

void RenderEngineBase::_LoopOverlay(IRenderData* pData, std::function<void(BufferLineRenderData&)> action)
{
    // First get the screen buffer's viewport.
    Viewport view = pData->GetViewport();
    const auto globalInvert = pData->IsScreenReversed();
    const auto gridLineDrawingAllowed = pData->IsGridLineDrawingAllowed();

    // Now get the overlay's viewport and adjust it to where it is supposed to be relative to the window.
    const auto overlays = pData->GetOverlays();
    for (const auto& overlay : overlays)
    {
        SMALL_RECT srCaView = overlay.region.ToInclusive();
        srCaView.Top += overlay.origin.Y;
        srCaView.Bottom += overlay.origin.Y;
        srCaView.Left += overlay.origin.X;
        srCaView.Right += overlay.origin.X;

        // Set it up in a Viewport helper structure and trim it the IME viewport to be within the full console viewport.
        Viewport viewConv = Viewport::FromInclusive(srCaView);

        gsl::span<const til::rectangle> dirtyAreas;
        LOG_IF_FAILED(GetDirtyArea(dirtyAreas));

        for (SMALL_RECT srDirty : dirtyAreas)
        {
            // Dirty is an inclusive rectangle, but oddly enough the IME was an exclusive one, so correct it.
            srDirty.Bottom++;
            srDirty.Right++;

            if (viewConv.TrimToViewport(&srDirty))
            {
                Viewport viewDirty = Viewport::FromInclusive(srDirty);

                for (SHORT iRow = viewDirty.Top(); iRow < viewDirty.BottomInclusive(); iRow++)
                {
                    const COORD target{ viewDirty.Left(), iRow };
                    const auto source = target - overlay.origin;

                    SMALL_RECT limit;
                    limit.Top = source.Y;
                    limit.Bottom = source.Y;
                    limit.Left = 0;
                    limit.Right = overlay.buffer.GetSize().RightInclusive();

                    BufferLineRenderData renderData{
                        pData,
                        overlay.buffer,
                        source,
                        Viewport::FromInclusive(limit),
                        target,
                        viewConv,
                        LineRendition::SingleWidth,
                        false,
                        false,
                        globalInvert,
                        gridLineDrawingAllowed
                    };
                    action(renderData);
                }
            }
        }
    }
}

void RenderEngineBase::_LoopSelection(IRenderData* pData, std::function<void(SMALL_RECT)> action)
{
    gsl::span<const til::rectangle> dirtyAreas;
    LOG_IF_FAILED(GetDirtyArea(dirtyAreas));

    // Get selection rectangles
    const auto rectangles = _GetSelectionRects(pData);
    for (auto rect : rectangles)
    {
        for (auto& dirtyRect : dirtyAreas)
        {
            // Make a copy as `TrimToViewport` will manipulate it and
            // can destroy it for the next dirtyRect to test against.
            auto rectCopy = rect;
            Viewport dirtyView = Viewport::FromInclusive(dirtyRect);
            if (dirtyView.TrimToViewport(&rectCopy))
            {
                action(rectCopy);
            }
        }
    }
}

IRenderEngine::GridLines RenderEngineBase::_CalculateGridLines(IRenderData* pData,
                                            const TextAttribute textAttribute,
                                            const COORD coordTarget) {
    // Convert console grid line representations into rendering engine enum representations.
    IRenderEngine::GridLines lines = s_GetGridlines(textAttribute);

    // For now, we dash underline patterns and switch to regular underline on hover
    // Since we're only rendering pattern links on *hover*, there's no point in checking
    // the pattern range if we aren't currently hovering.
    if (_hoveredInterval.has_value())
    {
        const til::point coordTargetTil{ coordTarget };
        if (_hoveredInterval->start <= coordTargetTil &&
            coordTargetTil <= _hoveredInterval->stop)
        {
            if (pData->GetPatternId(coordTarget).size() > 0)
            {
                lines |= IRenderEngine::GridLines::Underline;
            }
        }
    }

    return lines;
 }

// Method Description:
 // - Generates a IRenderEngine::GridLines structure from the values in the
 //      provided textAttribute
 // Arguments:
 // - textAttribute: the TextAttribute to generate GridLines from.
 // Return Value:
 // - a GridLines containing all the gridline info from the TextAttribute
 IRenderEngine::GridLines RenderEngineBase::s_GetGridlines(const TextAttribute& textAttribute) noexcept
 {
     // Convert console grid line representations into rendering engine enum representations.
     IRenderEngine::GridLines lines = IRenderEngine::GridLines::None;

     if (textAttribute.IsTopHorizontalDisplayed())
     {
         lines |= IRenderEngine::GridLines::Top;
     }

     if (textAttribute.IsBottomHorizontalDisplayed())
     {
         lines |= IRenderEngine::GridLines::Bottom;
     }

     if (textAttribute.IsLeftVerticalDisplayed())
     {
         lines |= IRenderEngine::GridLines::Left;
     }

     if (textAttribute.IsRightVerticalDisplayed())
     {
         lines |= IRenderEngine::GridLines::Right;
     }

     if (textAttribute.IsCrossedOut())
     {
         lines |= IRenderEngine::GridLines::Strikethrough;
     }

     if (textAttribute.IsUnderlined())
     {
         lines |= IRenderEngine::GridLines::Underline;
     }

     if (textAttribute.IsDoublyUnderlined())
     {
         lines |= IRenderEngine::GridLines::DoubleUnderline;
     }

     if (textAttribute.IsHyperlink())
     {
         lines |= IRenderEngine::GridLines::HyperlinkUnderline;
     }
     return lines;
 }

 // Routine Description:
 // - Helper to determine the selected region of the buffer.
 // Return Value:
 // - A vector of rectangles representing the regions to select, line by line.
 std::vector<SMALL_RECT> RenderEngineBase::_GetSelectionRects(IRenderData* pData) const
 {
     const auto& buffer = pData->GetTextBuffer();
     auto rects = pData->GetSelectionRects();
     // Adjust rectangles to viewport
     Viewport view = pData->GetViewport();

     std::vector<SMALL_RECT> result;

     for (auto rect : rects)
     {
         // Convert buffer offsets to the equivalent range of screen cells
         // expected by callers, taking line rendition into account.
         const auto lineRendition = buffer.GetLineRendition(rect.Top());
         rect = Viewport::FromInclusive(BufferToScreenLine(rect.ToInclusive(), lineRendition));

         auto sr = view.ConvertToOrigin(rect).ToInclusive();

         // hopefully temporary, we should be receiving the right selection sizes without correction.
         sr.Right++;
         sr.Bottom++;

         result.emplace_back(sr);
     }

     return result;
 }
