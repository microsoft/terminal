// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "renderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

static constexpr auto maxRetriesForRenderEngine = 3;
// The renderer will wait this number of milliseconds * how many tries have elapsed before trying again.
static constexpr auto renderBackoffBaseTimeMilliseconds{ 150 };

#define FOREACH_ENGINE(var)   \
    for (auto var : _engines) \
        if (!var)             \
            break;            \
        else

// Routine Description:
// - Creates a new renderer controller for a console.
// Arguments:
// - pData - The interface to console data structures required for rendering
// - pEngine - The output engine for targeting each rendering frame
// Return Value:
// - An instance of a Renderer.
Renderer::Renderer(const RenderSettings& renderSettings,
                   IRenderData* pData,
                   _In_reads_(cEngines) IRenderEngine** const rgpEngines,
                   const size_t cEngines,
                   std::unique_ptr<RenderThread> thread) :
    _renderSettings(renderSettings),
    _pData(pData),
    _pThread{ std::move(thread) }
{
    for (size_t i = 0; i < cEngines; i++)
    {
        AddRenderEngine(rgpEngines[i]);
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
    // RenderThread blocks until it has shut down.
    _destructing = true;
    _pThread.reset();
}

// Routine Description:
// - Walks through the console data structures to compose a new frame based on the data that has changed since last call and outputs it to the connected rendering engine.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK, GDI error, Safe Math error, or state/argument errors.
[[nodiscard]] HRESULT Renderer::PaintFrame()
{
    FOREACH_ENGINE(pEngine)
    {
        auto tries = maxRetriesForRenderEngine;
        while (tries > 0)
        {
            if (_destructing)
            {
                return S_FALSE;
            }

            const auto hr = _PaintFrameForEngine(pEngine);
            if (SUCCEEDED(hr))
            {
                break;
            }

            LOG_HR_IF(hr, hr != E_PENDING);

            if (--tries == 0)
            {
                // Stop trying.
                _pThread->DisablePainting();
                if (_pfnRendererEnteredErrorState)
                {
                    _pfnRendererEnteredErrorState();
                }
                // If there's no callback, we still don't want to FAIL_FAST: the renderer going black
                // isn't near as bad as the entire application aborting. We're a component. We shouldn't
                // abort applications that host us.
                return S_FALSE;
            }

            // Add a bit of backoff.
            // Sleep 150ms, 300ms, 450ms before failing out and disabling the renderer.
            Sleep(renderBackoffBaseTimeMilliseconds * (maxRetriesForRenderEngine - tries));
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT Renderer::_PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept
try
{
    FAIL_FAST_IF_NULL(pEngine); // This is a programming error. Fail fast.

    _pData->LockConsole();
    auto unlock = wil::scope_exit([&]() {
        _pData->UnlockConsole();
    });

    // Last chance check if anything scrolled without an explicit invalidate notification since the last frame.
    _CheckViewportAndScroll();

    // Try to start painting a frame
    const auto hr = pEngine->StartPaint();
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

        // If the engine tells us it really wants to redraw immediately,
        // tell the thread so it doesn't go to sleep and ticks again
        // at the next opportunity.
        if (pEngine->RequiresContinuousRedraw())
        {
            NotifyPaintFrame();
        }
    });

    // A. Prep Colors
    RETURN_IF_FAILED(_UpdateDrawingBrushes(pEngine, {}, false, true));

    // B. Perform Scroll Operations
    RETURN_IF_FAILED(_PerformScrolling(pEngine));

    // C. Prepare the engine with additional information before we start drawing.
    RETURN_IF_FAILED(_PrepareRenderInfo(pEngine));

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
CATCH_RETURN()

void Renderer::NotifyPaintFrame() noexcept
{
    // If we're running in the unittests, we might not have a render thread.
    if (_pThread)
    {
        // The thread will provide throttling for us.
        _pThread->NotifyPaint();
    }
}

// Routine Description:
// - Called when the system has requested we redraw a portion of the console.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSystemRedraw(const til::rect* const prcDirtyClient)
{
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->InvalidateSystem(prcDirtyClient));
    }

    NotifyPaintFrame();
}

// Routine Description:
// - Called when a particular region within the console buffer has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const Viewport& region)
{
    auto view = _viewport;
    auto srUpdateRegion = region.ToExclusive();

    // If the dirty region has double width lines, we need to double the size of
    // the right margin to make sure all the affected cells are invalidated.
    const auto& buffer = _pData->GetTextBuffer();
    for (auto row = srUpdateRegion.top; row < srUpdateRegion.bottom; row++)
    {
        if (buffer.IsDoubleWidthLine(row))
        {
            srUpdateRegion.right *= 2;
            break;
        }
    }

    if (view.TrimToViewport(&srUpdateRegion))
    {
        view.ConvertToOrigin(&srUpdateRegion);
        FOREACH_ENGINE(pEngine)
        {
            LOG_IF_FAILED(pEngine->Invalidate(&srUpdateRegion));
        }

        NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when a particular coordinate within the console buffer has changed.
// Arguments:
// - pcoord: The buffer-space coordinate that has changed.
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const til::point* const pcoord)
{
    TriggerRedraw(Viewport::FromCoord(*pcoord)); // this will notify to paint if we need it.
}

// Routine Description:
// - Called when the cursor has moved in the buffer. Allows for RenderEngines to
//      differentiate between cursor movements and other invalidates.
//   Visual Renderers (ex GDI) should invalidate the position, while the VT
//      engine ignores this. See MSFT:14711161.
// Arguments:
// - pcoord: The buffer-space position of the cursor.
// Return Value:
// - <none>
void Renderer::TriggerRedrawCursor(const til::point* const pcoord)
{
    // We first need to make sure the cursor position is within the buffer,
    // otherwise testing for a double width character can throw an exception.
    const auto& buffer = _pData->GetTextBuffer();
    if (buffer.GetSize().IsInBounds(*pcoord))
    {
        // We then calculate the region covered by the cursor. This requires
        // converting the buffer coordinates to an equivalent range of screen
        // cells for the cursor, taking line rendition into account.
        const auto lineRendition = buffer.GetLineRendition(pcoord->y);
        const auto cursorWidth = _pData->IsCursorDoubleWidth() ? 2 : 1;
        til::inclusive_rect cursorRect = { pcoord->x, pcoord->y, pcoord->x + cursorWidth - 1, pcoord->y };
        cursorRect = BufferToScreenLine(cursorRect, lineRendition);

        // That region is then clipped within the viewport boundaries and we
        // only trigger a redraw if the resulting region is not empty.
        auto view = _pData->GetViewport();
        auto updateRect = til::rect{ cursorRect };
        if (view.TrimToViewport(&updateRect))
        {
            view.ConvertToOrigin(&updateRect);
            FOREACH_ENGINE(pEngine)
            {
                LOG_IF_FAILED(pEngine->InvalidateCursor(&updateRect));
            }

            NotifyPaintFrame();
        }
    }
}

// Routine Description:
// - Called when something that changes the output state has occurred and the entire frame is now potentially invalid.
// - NOTE: Use sparingly. Try to reduce the refresh region where possible. Only use when a global state change has occurred.
// Arguments:
// - backgroundChanged - Set to true if the background color has changed.
// - frameChanged - Set to true if the frame colors have changed.
// Return Value:
// - <none>
void Renderer::TriggerRedrawAll(const bool backgroundChanged, const bool frameChanged)
{
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->InvalidateAll());
    }

    NotifyPaintFrame();

    if (backgroundChanged && _pfnBackgroundColorChanged)
    {
        _pfnBackgroundColorChanged();
    }

    if (frameChanged && _pfnFrameColorChanged)
    {
        _pfnFrameColorChanged();
    }
}

// Method Description:
// - Called when the host is about to die, to give the renderer one last chance
//      to paint before the host exits.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerTeardown() noexcept
{
    // We need to shut down the paint thread on teardown.
    _pThread->WaitForPaintCompletionAndDisable(INFINITE);

    // Then walk through and do one final paint on the caller's thread.
    FOREACH_ENGINE(pEngine)
    {
        auto fEngineRequestsRepaint = false;
        auto hr = pEngine->PrepareForTeardown(&fEngineRequestsRepaint);
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
        auto rects = _GetSelectionRects();

        // Make a viewport representing the coordinates that are currently presentable.
        const til::rect viewport{ _pData->GetViewport().Dimensions() };

        // Restrict all previous selection rectangles to inside the current viewport bounds
        for (auto& sr : _previousSelection)
        {
            sr &= viewport;
        }

        FOREACH_ENGINE(pEngine)
        {
            LOG_IF_FAILED(pEngine->InvalidateSelection(_previousSelection));
            LOG_IF_FAILED(pEngine->InvalidateSelection(rects));
        }

        _previousSelection = std::move(rects);

        NotifyPaintFrame();
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
    const auto srOldViewport = _viewport.ToInclusive();
    const auto srNewViewport = _pData->GetViewport().ToInclusive();

    if (!_forceUpdateViewport && srOldViewport == srNewViewport)
    {
        return false;
    }

    _viewport = Viewport::FromInclusive(srNewViewport);
    _forceUpdateViewport = false;

    til::point coordDelta;
    coordDelta.x = srOldViewport.left - srNewViewport.left;
    coordDelta.y = srOldViewport.top - srNewViewport.top;

    FOREACH_ENGINE(engine)
    {
        LOG_IF_FAILED(engine->UpdateViewport(srNewViewport));
        LOG_IF_FAILED(engine->InvalidateScroll(&coordDelta));
    }

    _ScrollPreviousSelection(coordDelta);
    return true;
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
        NotifyPaintFrame();
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
void Renderer::TriggerScroll(const til::point* const pcoordDelta)
{
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->InvalidateScroll(pcoordDelta));
    }

    _ScrollPreviousSelection(*pcoordDelta);

    NotifyPaintFrame();
}

// Routine Description:
// - Called when the text buffer is about to circle its backing buffer.
//      A renderer might want to get painted before that happens.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerFlush(const bool circling)
{
    const auto rects = _GetSelectionRects();

    FOREACH_ENGINE(pEngine)
    {
        auto fEngineRequestsRepaint = false;
        auto hr = pEngine->InvalidateFlush(circling, &fEngineRequestsRepaint);
        LOG_IF_FAILED(hr);

        LOG_IF_FAILED(pEngine->InvalidateSelection(rects));

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
    const auto newTitle = _pData->GetConsoleTitle();
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->InvalidateTitle(newTitle));
    }
    NotifyPaintFrame();
}

void Renderer::TriggerNewTextNotification(const std::wstring_view newText)
{
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->NotifyNewText(newText));
    }
}

// Routine Description:
// - Update the title for a particular engine.
// Arguments:
// - pEngine: the engine to update the title for.
// Return Value:
// - the HRESULT of the underlying engine's UpdateTitle call.
HRESULT Renderer::_PaintTitle(IRenderEngine* const pEngine)
{
    const auto newTitle = _pData->GetConsoleTitle();
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
    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->UpdateDpi(iDpi));
        LOG_IF_FAILED(pEngine->UpdateFont(FontInfoDesired, FontInfo));
    }

    NotifyPaintFrame();
}

// Routine Description:
// - Called when the active soft font has been updated.
// Arguments:
// - bitPattern - An array of scanlines representing all the glyphs in the font.
// - cellSize - The cell size for an individual glyph.
// - centeringHint - The horizontal extent that glyphs are offset from center.
// Return Value:
// - <none>
void Renderer::UpdateSoftFont(const std::span<const uint16_t> bitPattern, const til::size cellSize, const size_t centeringHint)
{
    // We reserve PUA code points U+EF20 to U+EF7F for soft fonts, but the range
    // that we test for in _IsSoftFontChar will depend on the size of the active
    // bitPattern. If it's empty (i.e. no soft font is set), then nothing will
    // match, and those code points will be treated the same as everything else.
    const auto softFontCharCount = cellSize.height ? bitPattern.size() / cellSize.height : 0;
    _lastSoftFontChar = _firstSoftFontChar + softFontCharCount - 1;

    FOREACH_ENGINE(pEngine)
    {
        LOG_IF_FAILED(pEngine->UpdateSoftFont(bitPattern, cellSize, centeringHint));
    }
    TriggerRedrawAll();
}

// We initially tried to have a "_isSoftFontChar" member function, but MSVC
// failed to inline it at _all_ call sites (check invocations inside loops).
// This issue strangely doesn't occur with static functions.
bool Renderer::s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar)
{
    return v.size() == 1 && v[0] >= firstSoftFontChar && v[0] <= lastSoftFontChar;
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
    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FOREACH_ENGINE(pEngine)
    {
        const auto hr = LOG_IF_FAILED(pEngine->GetProposedFont(FontInfoDesired, FontInfo, iDpi));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return hr;
        }
    }

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
    auto fIsFullWidth = false;

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FOREACH_ENGINE(pEngine)
    {
        const auto hr = LOG_IF_FAILED(pEngine->IsGlyphWideByFont(glyph, &fIsFullWidth));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            break;
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
    // When the renderer is constructed, the initial viewport won't be available yet,
    // but once EnablePainting is called it should be safe to retrieve.
    _viewport = _pData->GetViewport();
    _forceUpdateViewport = true;

    // When running the unit tests, we may be using a render without a render thread.
    if (_pThread)
    {
        _pThread->EnablePainting();
    }
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
// - See also: Helper functions that separate out each complexity of text rendering.
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
    std::span<const til::rect> dirtyAreas;
    LOG_IF_FAILED(pEngine->GetDirtyArea(dirtyAreas));

    // This is to make sure any transforms are reset when this paint is finished.
    auto resetLineTransform = wil::scope_exit([&]() {
        LOG_IF_FAILED(pEngine->ResetLineTransform());
    });

    for (const auto& dirtyRect : dirtyAreas)
    {
        if (!dirtyRect)
        {
            continue;
        }

        auto dirty = Viewport::FromExclusive(dirtyRect);

        // Shift the origin of the dirty region to match the underlying buffer so we can
        // compare the two regions directly for intersection.
        dirty = Viewport::Offset(dirty, view.Origin());

        // The intersection between what is dirty on the screen (in need of repaint)
        // and what is supposed to be visible on the screen (the viewport) is what
        // we need to walk through line-by-line and repaint onto the screen.
        const auto redraw = Viewport::Intersect(dirty, view);

        // Retrieve the text buffer so we can read information out of it.
        const auto& buffer = _pData->GetTextBuffer();

        // Now walk through each row of text that we need to redraw.
        for (auto row = redraw.Top(); row < redraw.BottomExclusive(); row++)
        {
            // Calculate the boundaries of a single line. This is from the left to right edge of the dirty
            // area in width and exactly 1 tall.
            const auto screenLine = til::inclusive_rect{ redraw.Left(), row, redraw.RightInclusive(), row };

            // Convert the screen coordinates of the line to an equivalent
            // range of buffer cells, taking line rendition into account.
            const auto lineRendition = buffer.GetLineRendition(row);
            const auto bufferLine = Viewport::FromInclusive(ScreenToBufferLine(screenLine, lineRendition));

            // Find where on the screen we should place this line information. This requires us to re-map
            // the buffer-based origin of the line back onto the screen-based origin of the line.
            // For example, the screen might say we need to paint line 1 because it is dirty but the viewport
            // is actually looking at line 26 relative to the buffer. This means that we need line 27 out
            // of the backing buffer to fill in line 1 of the screen.
            const auto screenPosition = bufferLine.Origin() - til::point{ 0, view.Top() };

            // Retrieve the cell information iterator limited to just this line we want to redraw.
            auto it = buffer.GetCellDataAt(bufferLine.Origin(), bufferLine);

            // Calculate if two things are true:
            // 1. this row wrapped
            // 2. We're painting the last col of the row.
            // In that case, set lineWrapped=true for the _PaintBufferOutputHelper call.
            const auto lineWrapped = (buffer.GetRowByOffset(bufferLine.Origin().y).WasWrapForced()) &&
                                     (bufferLine.RightExclusive() == buffer.GetSize().Width());

            // Prepare the appropriate line transform for the current row and viewport offset.
            LOG_IF_FAILED(pEngine->PrepareLineTransform(lineRendition, screenPosition.y, view.Left()));

            // Ask the helper to paint through this specific line.
            _PaintBufferOutputHelper(pEngine, it, screenPosition, lineWrapped);
        }
    }
}

static bool _IsAllSpaces(const std::wstring_view v)
{
    // first non-space char is not found (is npos)
    return v.find_first_not_of(L' ') == decltype(v)::npos;
}

void Renderer::_PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine,
                                        TextBufferCellIterator it,
                                        const til::point target,
                                        const bool lineWrapped)
{
    auto globalInvert{ _renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed) };

    // If we have valid data, let's figure out how to draw it.
    if (it)
    {
        // TODO: MSFT: 20961091 -  This is a perf issue. Instead of rebuilding this and allocing memory to hold the reinterpretation,
        // we should have an iterator/view adapter for the rendering.
        // That would probably also eliminate the RenderData needing to give us the entire TextBuffer as well...
        // Retrieve the iterator for one line of information.
        til::CoordType cols = 0;

        // Retrieve the first color.
        auto color = it->TextAttr();
        // Retrieve the first pattern id
        auto patternIds = _pData->GetPatternId(target);
        // Determine whether we're using a soft font.
        auto usingSoftFont = s_IsSoftFontChar(it->Chars(), _firstSoftFontChar, _lastSoftFontChar);

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

            // Hold onto the current pattern id as well
            const auto currentPatternId = patternIds;

            // Update the drawing brushes with our color and font usage.
            THROW_IF_FAILED(_UpdateDrawingBrushes(pEngine, currentRunColor, usingSoftFont, false));

            // Advance the point by however many columns we've just outputted and reset the accumulator.
            screenPoint.x += cols;
            cols = 0;

            // Hold onto the start of this run iterator and the target location where we started
            // in case we need to do some special work to paint the line drawing characters.
            const auto currentRunItStart = it;
            const auto currentRunTargetStart = screenPoint;

            // Ensure that our cluster vector is clear.
            _clusterBuffer.clear();

            // Reset our flag to know when we're in the special circumstance
            // of attempting to draw only the right-half of a two-column character
            // as the first item in our run.
            auto trimLeft = false;

            // Run contains wide character (>1 columns)
            auto containsWideCharacter = false;

            // This inner loop will accumulate clusters until the color changes.
            // When the color changes, it will save the new color off and break.
            // We also accumulate clusters according to regex patterns
            do
            {
                til::point thisPoint{ screenPoint.x + cols, screenPoint.y };
                const auto thisPointPatterns = _pData->GetPatternId(thisPoint);
                const auto thisUsingSoftFont = s_IsSoftFontChar(it->Chars(), _firstSoftFontChar, _lastSoftFontChar);
                const auto changedPatternOrFont = patternIds != thisPointPatterns || usingSoftFont != thisUsingSoftFont;
                if (color != it->TextAttr() || changedPatternOrFont)
                {
                    auto newAttr{ it->TextAttr() };
                    // foreground doesn't matter for runs of spaces (!)
                    // if we trick it . . . we call Paint far fewer times for cmatrix
                    if (!_IsAllSpaces(it->Chars()) || !newAttr.HasIdenticalVisualRepresentationForBlankSpace(color, globalInvert) || changedPatternOrFont)
                    {
                        color = newAttr;
                        patternIds = thisPointPatterns;
                        usingSoftFont = thisUsingSoftFont;
                        break; // vend this run
                    }
                }

                // Walk through the text data and turn it into rendering clusters.
                // Keep the columnCount as we go to improve performance over digging it out of the vector at the end.
                auto columnCount = it->Columns();

                // If we're on the first cluster to be added and it's marked as "trailing"
                // (a.k.a. the right half of a two column character), then we need some special handling.
                if (_clusterBuffer.empty() && it->DbcsAttr() == DbcsAttribute::Trailing)
                {
                    // Move left to the one so the whole character can be struck correctly.
                    --screenPoint.x;
                    // And tell the next function to trim off the left half of it.
                    trimLeft = true;
                    // And add one to the number of columns we expect it to take as we insert it.
                    ++columnCount;
                }

                if (columnCount > 1)
                {
                    containsWideCharacter = true;
                }

                // Advance the cluster and column counts.
                _clusterBuffer.emplace_back(it->Chars(), columnCount);
                it += std::max(it->Columns(), 1); // prevent infinite loop for no visible columns
                cols += columnCount;

            } while (it);

            // Do the painting.
            THROW_IF_FAILED(pEngine->PaintBufferLine({ _clusterBuffer.data(), _clusterBuffer.size() }, screenPoint, trimLeft, lineWrapped));

            // If we're allowed to do grid drawing, draw that now too (since it will be coupled with the color data)
            // We're only allowed to draw the grid lines under certain circumstances.
            if (_pData->IsGridLineDrawingAllowed())
            {
                // See GH: 803
                // If we found a wide character while we looped above, it's possible we skipped over the right half
                // attribute that could have contained different line information than the left half.
                if (containsWideCharacter)
                {
                    // Start from the original position in this run.
                    auto lineIt = currentRunItStart;
                    // Start from the original target in this run.
                    auto lineTarget = currentRunTargetStart;

                    // We need to go through the iterators again to ensure we get the lines associated with each
                    // exact column. The code above will condense two-column characters into one, but it is possible
                    // (like with the IME) that the line drawing characters will vary from the left to right half
                    // of a wider character.
                    // We could theoretically pre-pass for this in the loop above to be more efficient about walking
                    // the iterator, but I fear it would make the code even more confusing than it already is.
                    // Do that in the future if some WPR trace points you to this spot as super bad.
                    for (til::CoordType colsPainted = 0; colsPainted < cols; ++colsPainted, ++lineIt, ++lineTarget.x)
                    {
                        auto lines = lineIt->TextAttr();
                        _PaintBufferOutputGridLineHelper(pEngine, lines, 1, lineTarget);
                    }
                }
                else
                {
                    // If nothing exciting is going on, draw the lines in bulk.
                    _PaintBufferOutputGridLineHelper(pEngine, currentRunColor, cols, screenPoint);
                }
            }
        }
    }
}

// Method Description:
// - Generates a GridLines structure from the values in the
//      provided textAttribute
// Arguments:
// - textAttribute: the TextAttribute to generate GridLines from.
// Return Value:
// - a GridLineSet containing all the gridline info from the TextAttribute
GridLineSet Renderer::s_GetGridlines(const TextAttribute& textAttribute) noexcept
{
    // Convert console grid line representations into rendering engine enum representations.
    GridLineSet lines;

    if (textAttribute.IsTopHorizontalDisplayed())
    {
        lines.set(GridLines::Top);
    }

    if (textAttribute.IsBottomHorizontalDisplayed())
    {
        lines.set(GridLines::Bottom);
    }

    if (textAttribute.IsLeftVerticalDisplayed())
    {
        lines.set(GridLines::Left);
    }

    if (textAttribute.IsRightVerticalDisplayed())
    {
        lines.set(GridLines::Right);
    }

    if (textAttribute.IsCrossedOut())
    {
        lines.set(GridLines::Strikethrough);
    }

    if (textAttribute.IsUnderlined())
    {
        lines.set(GridLines::Underline);
    }

    if (textAttribute.IsDoublyUnderlined())
    {
        lines.set(GridLines::DoubleUnderline);
    }

    if (textAttribute.IsHyperlink())
    {
        lines.set(GridLines::HyperlinkUnderline);
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
                                                const til::point coordTarget)
{
    // Convert console grid line representations into rendering engine enum representations.
    auto lines = Renderer::s_GetGridlines(textAttribute);

    // For now, we dash underline patterns and switch to regular underline on hover
    if (_isHoveredHyperlink(textAttribute) || _isInHoveredInterval(coordTarget))
    {
        lines.reset(GridLines::HyperlinkUnderline);
        lines.set(GridLines::Underline);
    }

    // Return early if there are no lines to paint.
    if (lines.any())
    {
        // Get the current foreground color to render the lines.
        const auto rgb = _renderSettings.GetAttributeColors(textAttribute).first;
        // Draw the lines
        LOG_IF_FAILED(pEngine->PaintBufferGridLines(lines, rgb, cchLine, coordTarget));
    }
}

bool Renderer::_isHoveredHyperlink(const TextAttribute& textAttribute) const noexcept
{
    return _hyperlinkHoveredId && _hyperlinkHoveredId == textAttribute.GetHyperlinkId();
}

bool Renderer::_isInHoveredInterval(const til::point coordTarget) const noexcept
{
    return _hoveredInterval &&
           _hoveredInterval->start <= coordTarget && coordTarget <= _hoveredInterval->stop &&
           _pData->GetPatternId(coordTarget).size() > 0;
}

// Routine Description:
// - Retrieve information about the cursor, and pack it into a CursorOptions
//   which the render engine can use for painting the cursor.
// - If the cursor is "off", or the cursor is out of bounds of the viewport,
//   this will return nullopt (indicating the cursor shouldn't be painted this
//   frame)
// Arguments:
// - <none>
// Return Value:
// - nullopt if the cursor is off or out-of-frame, otherwise a CursorOptions
[[nodiscard]] std::optional<CursorOptions> Renderer::_GetCursorInfo()
{
    if (_pData->IsCursorVisible())
    {
        // Get cursor position in buffer
        auto coordCursor = _pData->GetCursorPosition();

        // GH#3166: Only draw the cursor if it's actually in the viewport. It
        // might be on the line that's in that partially visible row at the
        // bottom of the viewport, the space that's not quite a full line in
        // height. Since we don't draw that text, we shouldn't draw the cursor
        // there either.

        // The cursor is never rendered as double height, so we don't care about
        // the exact line rendition - only whether it's double width or not.
        const auto doubleWidth = _pData->GetTextBuffer().IsDoubleWidthLine(coordCursor.y);
        const auto lineRendition = doubleWidth ? LineRendition::DoubleWidth : LineRendition::SingleWidth;

        // We need to convert the screen coordinates of the viewport to an
        // equivalent range of buffer cells, taking line rendition into account.
        const auto view = ScreenToBufferLine(_pData->GetViewport().ToInclusive(), lineRendition);

        // Note that we allow the X coordinate to be outside the left border by 1 position,
        // because the cursor could still be visible if the focused character is double width.
        const auto xInRange = coordCursor.x >= view.left - 1 && coordCursor.x <= view.right;
        const auto yInRange = coordCursor.y >= view.top && coordCursor.y <= view.bottom;
        if (xInRange && yInRange)
        {
            // Adjust cursor Y offset to viewport.
            // The viewport X offset is saved in the options and handled with a transform.
            coordCursor.y -= view.top;

            auto cursorColor = _renderSettings.GetColorTableEntry(TextColor::CURSOR_COLOR);
            auto useColor = cursorColor != INVALID_COLOR;

            // Build up the cursor parameters including position, color, and drawing options
            CursorOptions options;
            options.coordCursor = coordCursor;
            options.viewportLeft = _pData->GetViewport().Left();
            options.lineRendition = lineRendition;
            options.ulCursorHeightPercent = _pData->GetCursorHeight();
            options.cursorPixelWidth = _pData->GetCursorPixelWidth();
            options.fIsDoubleWidth = _pData->IsCursorDoubleWidth();
            options.cursorType = _pData->GetCursorStyle();
            options.fUseColor = useColor;
            options.cursorColor = cursorColor;
            options.isOn = _pData->IsCursorOn();

            return { options };
        }
    }
    return std::nullopt;
}

// Routine Description:
// - Paint helper to draw the cursor within the buffer.
// Arguments:
// - engine - The render engine that we're targeting.
// Return Value:
// - <none>
void Renderer::_PaintCursor(_In_ IRenderEngine* const pEngine)
{
    const auto cursorInfo = _GetCursorInfo();
    if (cursorInfo.has_value())
    {
        LOG_IF_FAILED(pEngine->PaintCursor(cursorInfo.value()));
    }
}

// Routine Description:
// - Retrieves info from the render data to prepare the engine with, before the
//   frame is drawn. Some renderers might want to use this information to affect
//   later drawing decisions.
//   * Namely, the DX renderer uses this to know the cursor position and state
//     before PaintCursor is called, so it can draw the cursor underneath the
//     text.
// Arguments:
// - engine - The render engine that we're targeting.
// Return Value:
// - S_OK if the engine prepared successfully, or a relevant error via HRESULT.
[[nodiscard]] HRESULT Renderer::_PrepareRenderInfo(_In_ IRenderEngine* const pEngine)
{
    RenderFrameInfo info;
    info.cursorInfo = _GetCursorInfo();
    return pEngine->PrepareRenderInfo(info);
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
        // Now get the overlay's viewport and adjust it to where it is supposed to be relative to the window.
        auto srCaView = overlay.region.ToExclusive();
        srCaView.top += overlay.origin.y;
        srCaView.bottom += overlay.origin.y;
        srCaView.left += overlay.origin.x;
        srCaView.right += overlay.origin.x;

        std::span<const til::rect> dirtyAreas;
        LOG_IF_FAILED(engine.GetDirtyArea(dirtyAreas));

        for (const auto& rect : dirtyAreas)
        {
            if (const auto viewDirty = rect & srCaView)
            {
                for (auto iRow = viewDirty.top; iRow < viewDirty.bottom; iRow++)
                {
                    const til::point target{ viewDirty.left, iRow };
                    const auto source = target - overlay.origin;

                    auto it = overlay.buffer.GetCellLineDataAt(source);

                    _PaintBufferOutputHelper(&engine, it, target, false);
                }
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
        std::span<const til::rect> dirtyAreas;
        LOG_IF_FAILED(pEngine->GetDirtyArea(dirtyAreas));

        // Get selection rectangles
        const auto rectangles = _GetSelectionRects();
        for (const auto& rect : rectangles)
        {
            for (auto& dirtyRect : dirtyAreas)
            {
                if (const auto rectCopy = rect & dirtyRect)
                {
                    LOG_IF_FAILED(pEngine->PaintSelection(rectCopy));
                }
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
// - usingSoftFont - Whether we're rendering characters from a soft font
// - isSettingDefaultBrushes - Alerts that the default brushes are being set which will
//                             impact whether or not to include the hung window/erase window brushes in this operation
//                             and can affect other draw state that wants to know the default color scheme.
//                             (Usually only happens when the default is changed, not when each individual color is swapped in a multi-color run.)
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine,
                                                      const TextAttribute textAttributes,
                                                      const bool usingSoftFont,
                                                      const bool isSettingDefaultBrushes)
{
    // The last color needs to be each engine's responsibility. If it's local to this function,
    //      then on the next engine we might not update the color.
    return pEngine->UpdateDrawingBrushes(textAttributes, _renderSettings, _pData, usingSoftFont, isSettingDefaultBrushes);
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
std::vector<til::rect> Renderer::_GetSelectionRects() const
{
    const auto& buffer = _pData->GetTextBuffer();
    auto rects = _pData->GetSelectionRects();
    // Adjust rectangles to viewport
    auto view = _pData->GetViewport();

    std::vector<til::rect> result;
    result.reserve(rects.size());

    for (auto rect : rects)
    {
        // Convert buffer offsets to the equivalent range of screen cells
        // expected by callers, taking line rendition into account.
        const auto lineRendition = buffer.GetLineRendition(rect.Top());
        rect = Viewport::FromInclusive(BufferToScreenLine(rect.ToInclusive(), lineRendition));
        result.emplace_back(view.ConvertToOrigin(rect).ToExclusive());
    }

    return result;
}

// Method Description:
// - Offsets all of the selection rectangles we might be holding onto
//   as the previously selected area. If the whole viewport scrolls,
//   we need to scroll these areas also to ensure they're invalidated
//   properly when the selection further changes.
// Arguments:
// - delta - The scroll delta
// Return Value:
// - <none> - Updates internal state instead.
void Renderer::_ScrollPreviousSelection(const til::point delta)
{
    if (delta != til::point{ 0, 0 })
    {
        for (auto& rc : _previousSelection)
        {
            rc += delta;
        }
    }
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
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);

    for (auto& p : _engines)
    {
        if (!p)
        {
            p = pEngine;
            return;
        }
    }

    THROW_HR_MSG(E_UNEXPECTED, "engines array is full");
}

void Renderer::RemoveRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);

    for (auto& p : _engines)
    {
        if (p == pEngine)
        {
            p = nullptr;
            return;
        }
    }
}

// Method Description:
// - Registers a callback for when the background color is changed
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetBackgroundColorChangedCallback(std::function<void()> pfn)
{
    _pfnBackgroundColorChanged = std::move(pfn);
}

// Method Description:
// - Registers a callback for when the frame colors have changed
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetFrameColorChangedCallback(std::function<void()> pfn)
{
    _pfnFrameColorChanged = std::move(pfn);
}

// Method Description:
// - Registers a callback that will be called when this renderer gives up.
//   An application consuming a renderer can use this to display auxiliary Retry UI
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetRendererEnteredErrorStateCallback(std::function<void()> pfn)
{
    _pfnRendererEnteredErrorState = std::move(pfn);
}

// Method Description:
// - Attempts to restart the renderer.
void Renderer::ResetErrorStateAndResume()
{
    // because we're not stateful (we could be in the future), all we want to do is reenable painting.
    EnablePainting();
}

void Renderer::UpdateHyperlinkHoveredId(uint16_t id) noexcept
{
    _hyperlinkHoveredId = id;
    FOREACH_ENGINE(pEngine)
    {
        pEngine->UpdateHyperlinkHoveredId(id);
    }
}

void Renderer::UpdateLastHoveredInterval(const std::optional<PointTree::interval>& newInterval)
{
    _hoveredInterval = newInterval;
}

// Method Description:
// - Blocks until the engines are able to render without blocking.
void Renderer::WaitUntilCanRender()
{
    FOREACH_ENGINE(pEngine)
    {
        pEngine->WaitUntilCanRender();
    }
}
