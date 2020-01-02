// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "renderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

static constexpr auto maxRetriesForRenderEngine = 3;

// Routine Description:
// - Creates a new renderer controller for a console.
// Arguments:
// - pData - The interface to console data structures required for rendering
// - pEngine - The output engine for targeting each rendering frame
// Return Value:
// - An instance of a Renderer.
// NOTE: CAN THROW IF MEMORY ALLOCATION FAILS.
Renderer::Renderer(IRenderData* pData,
                   _In_reads_(cEngines) IRenderEngine** const rgpEngines,
                   const size_t cEngines,
                   std::unique_ptr<IRenderThread> thread) :
    _pData(pData),
    _pThread{ std::move(thread) },
    _destructing{ false }
{
    _srViewportPrevious = { 0 };

    for (size_t i = 0; i < cEngines; i++)
    {
        IRenderEngine* engine = rgpEngines[i];
        // NOTE: THIS CAN THROW IF MEMORY ALLOCATION FAILS.
        AddRenderEngine(engine);
    }
}

// Routine Description:
// - Destroys an instance of a renderer
// Arguments:
// - <none>
// Return Value:
// - <none>
Renderer::~Renderer()
{
    _destructing = true;
}

// Routine Description:
// - Walks through the console data structures to compose a new frame based on the data that has changed since last call and outputs it to the connected rendering engine.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK, GDI error, Safe Math error, or state/argument errors.
[[nodiscard]] HRESULT Renderer::PaintFrame()
{
    if (_destructing)
    {
        return S_FALSE;
    }

    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        auto tries = maxRetriesForRenderEngine;
        while (tries > 0)
        {
            const auto hr = _PaintFrameForEngine(pEngine);
            if (E_PENDING == hr)
            {
                if (--tries == 0)
                {
                    FAIL_FAST_HR_MSG(E_UNEXPECTED, "A rendering engine required too many retries.");
                }
                continue;
            }
            LOG_IF_FAILED(hr);
            break;
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT Renderer::_PaintFrameForEngine(_In_ IRenderEngine* const pEngine)
{
    FAIL_FAST_IF_NULL(pEngine); // This is a programming error. Fail fast.

    _pData->LockConsole();
    auto unlock = wil::scope_exit([&]() {
        _pData->UnlockConsole();
    });

    // Last chance check if anything scrolled without an explicit invalidate notification since the last frame.
    _CheckViewportAndScroll();

    // Try to start painting a frame
    HRESULT const hr = pEngine->StartPaint();
    RETURN_IF_FAILED(hr);

    // Return early if there's nothing to paint.
    // The renderer itself tracks if there's something to do with the title, the
    //      engine won't know that.
    if (S_FALSE == hr)
    {
        return S_OK;
    }

    auto endPaint = wil::scope_exit([&]() {
        LOG_IF_FAILED(pEngine->EndPaint());
    });

    // A. Prep Colors
    RETURN_IF_FAILED(_UpdateDrawingBrushes(pEngine, _pData->GetDefaultBrushColors(), true));

    // B. Perform Scroll Operations
    RETURN_IF_FAILED(_PerformScrolling(pEngine));

    // 1. Paint Background
    RETURN_IF_FAILED(_PaintBackground(pEngine));

    // 2. Paint Rows of Text
    _PaintBufferOutput(pEngine);

    // 3. Paint overlays that reside above the text buffer
    _PaintOverlays(pEngine);

    // 4. Paint Selection
    _PaintSelection(pEngine);

    // 5. Paint Cursor
    _PaintCursor(pEngine);

    // 6. Paint window title
    RETURN_IF_FAILED(_PaintTitle(pEngine));

    // Force scope exit end paint to finish up collecting information and possibly painting
    endPaint.reset();

    // Force scope exit unlock to let go of global lock so other threads can run
    unlock.reset();

    // Trigger out-of-lock presentation for renderers that can support it
    RETURN_IF_FAILED(pEngine->Present());

    // As we leave the scope, EndPaint will be called (declared above)
    return S_OK;
}

void Renderer::_NotifyPaintFrame()
{
    // The thread will provide throttling for us.
    _pThread->NotifyPaint();
}

// Routine Description:
// - Called when the system has requested we redraw a portion of the console.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSystemRedraw(const RECT* const prcDirtyClient)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->InvalidateSystem(prcDirtyClient));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Called when a particular region within the console buffer has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const Viewport& region)
{
    Viewport view = _pData->GetViewport();
    SMALL_RECT srUpdateRegion = region.ToExclusive();

    if (view.TrimToViewport(&srUpdateRegion))
    {
        view.ConvertToOrigin(&srUpdateRegion);
        std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
            LOG_IF_FAILED(pEngine->Invalidate(&srUpdateRegion));
        });

        _NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when a particular coordinate within the console buffer has changed.
// Arguments:
// - pcoord: The buffer-space coordinate that has changed.
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const COORD* const pcoord)
{
    TriggerRedraw(Viewport::FromCoord(*pcoord)); // this will notify to paint if we need it.
}

// Routine Description:
// - Called when the cursor has moved in the buffer. Allows for RenderEngines to
//      differentiate between cursor movements and other invalidates.
//   Visual Renderers (ex GDI) sohuld invalidate the position, while the VT
//      engine ignores this. See MSFT:14711161.
// Arguments:
// - pcoord: The buffer-space position of the cursor.
// Return Value:
// - <none>
void Renderer::TriggerRedrawCursor(const COORD* const pcoord)
{
    Viewport view = _pData->GetViewport();
    COORD updateCoord = *pcoord;

    if (view.IsInBounds(updateCoord))
    {
        view.ConvertToOrigin(&updateCoord);
        for (IRenderEngine* pEngine : _rgpEngines)
        {
            LOG_IF_FAILED(pEngine->InvalidateCursor(&updateCoord));

            // Double-wide cursors need to invalidate the right half as well.
            if (_pData->IsCursorDoubleWidth())
            {
                updateCoord.X++;
                LOG_IF_FAILED(pEngine->InvalidateCursor(&updateCoord));
            }
        }

        _NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when something that changes the output state has occurred and the entire frame is now potentially invalid.
// - NOTE: Use sparingly. Try to reduce the refresh region where possible. Only use when a global state change has occurred.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedrawAll()
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->InvalidateAll());
    });

    _NotifyPaintFrame();
}

// Method Description:
// - Called when the host is about to die, to give the renderer one last chance
//      to paint before the host exits.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerTeardown()
{
    // We need to shut down the paint thread on teardown.
    _pThread->WaitForPaintCompletionAndDisable(INFINITE);

    // Then walk through and do one final paint on the caller's thread.
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        bool fEngineRequestsRepaint = false;
        HRESULT hr = pEngine->PrepareForTeardown(&fEngineRequestsRepaint);
        LOG_IF_FAILED(hr);

        if (SUCCEEDED(hr) && fEngineRequestsRepaint)
        {
            LOG_IF_FAILED(_PaintFrameForEngine(pEngine));
        }
    }
}

// Routine Description:
// - Called when the selected area in the console has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSelection()
{
    try
    {
        // Get selection rectangles
        const auto rects = _GetSelectionRects();

        std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
            LOG_IF_FAILED(pEngine->InvalidateSelection(_previousSelection));
            LOG_IF_FAILED(pEngine->InvalidateSelection(rects));
        });

        _previousSelection = rects;

        _NotifyPaintFrame();
    }
    CATCH_LOG();
}

// Routine Description:
// - Called when we want to check if the viewport has moved and scroll accordingly if so.
// Arguments:
// - <none>
// Return Value:
// - True if something changed and we scrolled. False otherwise.
bool Renderer::_CheckViewportAndScroll()
{
    SMALL_RECT const srOldViewport = _srViewportPrevious;
    SMALL_RECT const srNewViewport = _pData->GetViewport().ToInclusive();

    COORD coordDelta;
    coordDelta.X = srOldViewport.Left - srNewViewport.Left;
    coordDelta.Y = srOldViewport.Top - srNewViewport.Top;

    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->UpdateViewport(srNewViewport));
        LOG_IF_FAILED(pEngine->InvalidateScroll(&coordDelta));
    });
    _srViewportPrevious = srNewViewport;

    return coordDelta.X != 0 || coordDelta.Y != 0;
}

// Routine Description:
// - Called when a scroll operation has occurred by manipulating the viewport.
// - This is a special case as calling out scrolls explicitly drastically improves performance.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerScroll()
{
    if (_CheckViewportAndScroll())
    {
        _NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when a scroll operation wishes to explicitly adjust the frame by the given coordinate distance.
// - This is a special case as calling out scrolls explicitly drastically improves performance.
// - This should only be used when the viewport is not modified. It lets us know we can "scroll anyway" to save perf,
//   because the backing circular buffer rotated out from behind the viewport.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerScroll(const COORD* const pcoordDelta)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->InvalidateScroll(pcoordDelta));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Called when the text buffer is about to circle its backing buffer.
//      A renderer might want to get painted before that happens.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerCircling()
{
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        bool fEngineRequestsRepaint = false;
        HRESULT hr = pEngine->InvalidateCircling(&fEngineRequestsRepaint);
        LOG_IF_FAILED(hr);

        if (SUCCEEDED(hr) && fEngineRequestsRepaint)
        {
            LOG_IF_FAILED(_PaintFrameForEngine(pEngine));
        }
    }
}

// Routine Description:
// - Called when the title of the console window has changed. Indicates that we
//      should update the title on the next frame.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerTitleChange()
{
    const std::wstring newTitle = _pData->GetConsoleTitle();
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        LOG_IF_FAILED(pEngine->InvalidateTitle(newTitle));
    }
    _NotifyPaintFrame();
}

// Routine Description:
// - Update the title for a particular engine.
// Arguments:
// - pEngine: the engine to update the title for.
// Return Value:
// - the HRESULT of the underlying engine's UpdateTitle call.
HRESULT Renderer::_PaintTitle(IRenderEngine* const pEngine)
{
    const std::wstring newTitle = _pData->GetConsoleTitle();
    return pEngine->UpdateTitle(newTitle);
}

// Routine Description:
// - Called when a change in font or DPI has been detected.
// Arguments:
// - iDpi - New DPI value
// - FontInfoDesired - A description of the font we would like to have.
// - FontInfo - Data that will be fixed up/filled on return with the chosen font data.
// Return Value:
// - <none>
void Renderer::TriggerFontChange(const int iDpi, const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->UpdateDpi(iDpi));
        LOG_IF_FAILED(pEngine->UpdateFont(FontInfoDesired, FontInfo));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Get the information on what font we would be using if we decided to create a font with the given parameters
// - This is for use with speculative calculations.
// Arguments:
// - iDpi - The DPI of the target display
// - pFontInfoDesired - A description of the font we would like to have.
// - pFontInfo - Data that will be fixed up/filled on return with the chosen font data.
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT Renderer::GetProposedFont(const int iDpi, const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo)
{
    // If there's no head, return E_FAIL. The caller should decide how to
    //      handle this.
    // Currently, the only caller is the WindowProc:WM_GETDPISCALEDSIZE handler.
    //      It will assume that the proposed font is 1x1, regardless of DPI.
    if (_rgpEngines.size() < 1)
    {
        return E_FAIL;
    }

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FAIL_FAST_IF(!(_rgpEngines.size() <= 2));
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        const HRESULT hr = LOG_IF_FAILED(pEngine->GetProposedFont(FontInfoDesired, FontInfo, iDpi));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return hr;
        }
    };

    return E_FAIL;
}

// Routine Description:
// - Tests against the current rendering engine to see if this particular character would be considered
// full-width (inscribed in a square, twice as wide as a standard Western character, typically used for CJK
// languages) or half-width.
// - Typically used to determine how many positions in the backing buffer a particular character should fill.
// NOTE: This only handles 1 or 2 wide (in monospace terms) characters.
// Arguments:
// - glyph - the utf16 encoded codepoint to test
// Return Value:
// - True if the codepoint is full-width (two wide), false if it is half-width (one wide).
bool Renderer::IsGlyphWideByFont(const std::wstring_view glyph)
{
    bool fIsFullWidth = false;

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FAIL_FAST_IF(!(_rgpEngines.size() <= 2));
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        const HRESULT hr = LOG_IF_FAILED(pEngine->IsGlyphWideByFont(glyph, &fIsFullWidth));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return fIsFullWidth;
        }
    }

    return fIsFullWidth;
}

// Routine Description:
// - Sets an event in the render thread that allows it to proceed, thus enabling painting.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::EnablePainting()
{
    _pThread->EnablePainting();
}

// Routine Description:
// - Waits for the current paint operation to complete, if any, up to the specified timeout.
// - Resets an event in the render thread that precludes it from advancing, thus disabling rendering.
// - If no paint operation is currently underway, returns immediately.
// Arguments:
// - dwTimeoutMs - Milliseconds to wait for the current paint operation to complete, if any (can be INFINITE).
// Return Value:
// - <none>
void Renderer::WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs)
{
    _pThread->WaitForPaintCompletionAndDisable(dwTimeoutMs);
}

// Routine Description:
// - Paint helper to fill in the background color of the invalid area within the frame.
// Arguments:
// - <none>
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_PaintBackground(_In_ IRenderEngine* const pEngine)
{
    return pEngine->PaintBackground();
}

// Routine Description:
// - Paint helper to copy the primary console buffer text onto the screen.
// - This portion primarily handles figuring the current viewport, comparing it/trimming it versus the invalid portion of the frame, and queuing up, row by row, which pieces of text need to be further processed.
// - See also: Helper functions that seperate out each complexity of text rendering.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintBufferOutput(_In_ IRenderEngine* const pEngine)
{
    // This is the subsection of the entire screen buffer that is currently being presented.
    // It can move left/right or top/bottom depending on how the viewport is scrolled
    // relative to the entire buffer.
    const auto view = _pData->GetViewport();

    // This is effectively the number of cells on the visible screen that need to be redrawn.
    // The origin is always 0, 0 because it represents the screen itself, not the underlying buffer.
    auto dirty = Viewport::FromInclusive(pEngine->GetDirtyRectInChars());

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
        const auto& buffer = _pData->GetTextBuffer();

        // Now walk through each row of text that we need to redraw.
        for (auto row = redraw.Top(); row < redraw.BottomExclusive(); row++)
        {
            // Calculate the boundaries of a single line. This is from the left to right edge of the dirty
            // area in width and exactly 1 tall.
            const auto bufferLine = Viewport::FromDimensions({ redraw.Left(), row }, { redraw.Width(), 1 });

            // Find where on the screen we should place this line information. This requires us to re-map
            // the buffer-based origin of the line back onto the screen-based origin of the line
            // For example, the screen might say we need to paint 1,1 because it is dirty but the viewport is actually looking
            // at 13,26 relative to the buffer.
            // This means that we need 14,27 out of the backing buffer to fill in the 1,1 cell of the screen.
            const auto screenLine = Viewport::Offset(bufferLine, -view.Origin());

            // Retrieve the cell information iterator limited to just this line we want to redraw.
            auto it = buffer.GetCellDataAt(bufferLine.Origin(), bufferLine);

            // Ask the helper to paint through this specific line.
            _PaintBufferOutputHelper(pEngine, it, screenLine.Origin());
        }
    }
}

void Renderer::_PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine,
                                        TextBufferCellIterator it,
                                        const COORD target)
{
    // If we have valid data, let's figure out how to draw it.
    if (it)
    {
        // TODO: MSFT: 20961091 -  This is a perf issue. Instead of rebuilding this and allocing memory to hold the reinterpretation,
        // we should have an iterator/view adapter for the rendering.
        // That would probably also eliminate the RenderData needing to give us the entire TextBuffer as well...
        // Retrieve the iterator for one line of information.
        std::vector<Cluster> clusters;
        size_t cols = 0;

        // Retrieve the first color.
        auto color = it->TextAttr();

        // And hold the point where we should start drawing.
        auto screenPoint = target;

        // This outer loop will continue until we reach the end of the text we are trying to draw.
        while (it)
        {
            // Hold onto the current run color right here for the length of the outer loop.
            // We'll be changing the persistent one as we run through the inner loops to detect
            // when a run changes, but we will still need to know this color at the bottom
            // when we go to draw gridlines for the length of the run.
            const auto currentRunColor = color;

            // Update the drawing brushes with our color.
            THROW_IF_FAILED(_UpdateDrawingBrushes(pEngine, currentRunColor, false));

            // Advance the point by however many columns we've just outputted and reset the accumulator.
            screenPoint.X += gsl::narrow<SHORT>(cols);
            cols = 0;

            // Ensure that our cluster vector is clear.
            clusters.clear();

            // This inner loop will accumulate clusters until the color changes.
            // When the color changes, it will save the new color off and break.
            do
            {
                if (color != it->TextAttr())
                {
                    color = it->TextAttr();
                    break;
                }

                // Walk through the text data and turn it into rendering clusters.
                clusters.emplace_back(it->Chars(), it->Columns());

                // Advance the cluster and column counts.
                const auto columnCount = clusters.back().GetColumns();
                it += columnCount > 0 ? columnCount : 1; // prevent infinite loop for no visible columns
                cols += columnCount;

            } while (it);

            // Do the painting.
            // TODO: Calculate when trim left should be TRUE
            THROW_IF_FAILED(pEngine->PaintBufferLine({ clusters.data(), clusters.size() }, screenPoint, false));

            // If we're allowed to do grid drawing, draw that now too (since it will be coupled with the color data)
            if (_pData->IsGridLineDrawingAllowed())
            {
                // We're only allowed to draw the grid lines under certain circumstances.
                _PaintBufferOutputGridLineHelper(pEngine, currentRunColor, cols, screenPoint);
            }
        }
    }
}

// Method Description:
// - Generates a IRenderEngine::GridLines structure from the values in the
//      provided textAttribute
// Arguments:
// - textAttribute: the TextAttribute to generate GridLines from.
// Return Value:
// - a GridLines containing all the gridline info from the TextAtribute
IRenderEngine::GridLines Renderer::s_GetGridlines(const TextAttribute& textAttribute) noexcept
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
    return lines;
}

// Routine Description:
// - Paint helper for primary buffer output function.
// - This particular helper sets up the various box drawing lines that can be inscribed around any character in the buffer (left, right, top, underline).
// - See also: All related helpers and buffer output functions.
// Arguments:
// - textAttribute - The line/box drawing attributes to use for this particular run.
// - cchLine - The length of both pwsLine and pbKAttrsLine.
// - coordTarget - The X/Y coordinate position in the buffer which we're attempting to start rendering from.
// Return Value:
// - <none>
void Renderer::_PaintBufferOutputGridLineHelper(_In_ IRenderEngine* const pEngine,
                                                const TextAttribute textAttribute,
                                                const size_t cchLine,
                                                const COORD coordTarget)
{
    const COLORREF rgb = _pData->GetForegroundColor(textAttribute);

    // Convert console grid line representations into rendering engine enum representations.
    IRenderEngine::GridLines lines = Renderer::s_GetGridlines(textAttribute);

    // Draw the lines
    LOG_IF_FAILED(pEngine->PaintBufferGridLines(lines, rgb, cchLine, coordTarget));
}

// Routine Description:
// - Paint helper to draw the cursor within the buffer.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintCursor(_In_ IRenderEngine* const pEngine)
{
    if (_pData->IsCursorVisible())
    {
        // Get cursor position in buffer
        COORD coordCursor = _pData->GetCursorPosition();
        // Adjust cursor to viewport
        Viewport view = _pData->GetViewport();
        view.ConvertToOrigin(&coordCursor);

        COLORREF cursorColor = _pData->GetCursorColor();
        bool useColor = cursorColor != INVALID_COLOR;

        // Build up the cursor parameters including position, color, and drawing options
        IRenderEngine::CursorOptions options;
        options.coordCursor = coordCursor;
        options.ulCursorHeightPercent = _pData->GetCursorHeight();
        options.cursorPixelWidth = _pData->GetCursorPixelWidth();
        options.fIsDoubleWidth = _pData->IsCursorDoubleWidth();
        options.cursorType = _pData->GetCursorStyle();
        options.fUseColor = useColor;
        options.cursorColor = cursorColor;
        options.isOn = _pData->IsCursorOn();

        // Draw it within the viewport
        LOG_IF_FAILED(pEngine->PaintCursor(options));
    }
}

// Routine Description:
// - Paint helper to draw text that overlays the main buffer to provide user interactivity regions
// - This supports IME composition.
// Arguments:
// - engine - The render engine that we're targeting.
// - overlay - The overlay to draw.
// Return Value:
// - <none>
void Renderer::_PaintOverlay(IRenderEngine& engine,
                             const RenderOverlay& overlay)
{
    try
    {
        // First get the screen buffer's viewport.
        Viewport view = _pData->GetViewport();

        // Now get the overlay's viewport and adjust it to where it is supposed to be relative to the window.

        SMALL_RECT srCaView = overlay.region.ToInclusive();
        srCaView.Top += overlay.origin.Y;
        srCaView.Bottom += overlay.origin.Y;
        srCaView.Left += overlay.origin.X;
        srCaView.Right += overlay.origin.X;

        // Set it up in a Viewport helper structure and trim it the IME viewport to be within the full console viewport.
        Viewport viewConv = Viewport::FromInclusive(srCaView);

        SMALL_RECT srDirty = engine.GetDirtyRectInChars();

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

                auto it = overlay.buffer.GetCellLineDataAt(source);

                _PaintBufferOutputHelper(&engine, it, target);
            }
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Paint helper to draw the composition string portion of the IME.
// - This specifically is the string that appears at the cursor on the input line showing what the user is currently typing.
// - See also: Generic Paint IME helper method.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintOverlays(_In_ IRenderEngine* const pEngine)
{
    try
    {
        const auto overlays = _pData->GetOverlays();

        for (const auto& overlay : overlays)
        {
            _PaintOverlay(*pEngine, overlay);
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Paint helper to draw the selected area of the window.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintSelection(_In_ IRenderEngine* const pEngine)
{
    try
    {
        SMALL_RECT srDirty = pEngine->GetDirtyRectInChars();
        Viewport dirtyView = Viewport::FromInclusive(srDirty);

        // Get selection rectangles
        const auto rectangles = _GetSelectionRects();
        for (auto rect : rectangles)
        {
            if (dirtyView.TrimToViewport(&rect))
            {
                LOG_IF_FAILED(pEngine->PaintSelection(rect));
            }
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Helper to convert the text attributes to actual RGB colors and update the rendering pen/brush within the rendering engine before the next draw operation.
// Arguments:
// - pEngine - Which engine is being updated
// - textAttributes - The 16 color foreground/background combination to set
// - isSettingDefaultBrushes - Alerts that the default brushes are being set which will
//                             impact whether or not to include the hung window/erase window brushes in this operation
//                             and can affect other draw state that wants to know the default color scheme.
//                             (Usually only happens when the default is changed, not when each individual color is swapped in a multi-color run.)
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine, const TextAttribute textAttributes, const bool isSettingDefaultBrushes)
{
    const COLORREF rgbForeground = _pData->GetForegroundColor(textAttributes);
    const COLORREF rgbBackground = _pData->GetBackgroundColor(textAttributes);
    const WORD legacyAttributes = textAttributes.GetLegacyAttributes();
    const auto extendedAttrs = textAttributes.GetExtendedAttributes();

    // The last color needs to be each engine's responsibility. If it's local to this function,
    //      then on the next engine we might not update the color.
    RETURN_IF_FAILED(pEngine->UpdateDrawingBrushes(rgbForeground, rgbBackground, legacyAttributes, extendedAttrs, isSettingDefaultBrushes));

    return S_OK;
}

// Routine Description:
// - Helper called before a majority of paint operations to scroll most of the previous frame into the appropriate
//   position before we paint the remaining invalid area.
// - Used to save drawing time/improve performance
// Arguments:
// - <none>
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_PerformScrolling(_In_ IRenderEngine* const pEngine)
{
    return pEngine->ScrollFrame();
}

// Routine Description:
// - Helper to determine the selected region of the buffer.
// Return Value:
// - A vector of rectangles representing the regions to select, line by line.
std::vector<SMALL_RECT> Renderer::_GetSelectionRects() const
{
    auto rects = _pData->GetSelectionRects();
    // Adjust rectangles to viewport
    Viewport view = _pData->GetViewport();

    std::vector<SMALL_RECT> result;

    for (auto& rect : rects)
    {
        auto sr = view.ConvertToOrigin(rect).ToInclusive();

        // hopefully temporary, we should be receiving the right selection sizes without correction.
        sr.Right++;
        sr.Bottom++;

        result.emplace_back(sr);
    }

    return result;
}

// Method Description:
// - Adds another Render engine to this renderer. Future rendering calls will
//      also be sent to the new renderer.
// Arguments:
// - pEngine: The new render engine to be added
// Return Value:
// - <none>
// Throws if we ran out of memory or there was some other error appending the
//      engine to our collection.
void Renderer::AddRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_IF_NULL_ALLOC(pEngine);
    _rgpEngines.push_back(pEngine);
}
