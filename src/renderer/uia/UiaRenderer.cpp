// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "UiaRenderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

// Routine Description:
// - Constructs a UIA engine for console text
//   which primarily notifies automation clients of any activity
UiaEngine::UiaEngine(IUiaEventDispatcher* dispatcher) :
    _dispatcher{ THROW_HR_IF_NULL(E_INVALIDARG, dispatcher) },
    _isPainting{ false },
    _selectionChanged{ false },
    _textBufferChanged{ false },
    _cursorChanged{ false },
    _isEnabled{ true },
    _prevSelection{},
    _prevCursorRegion{},
    RenderEngineBase()
{
}

// Routine Description:
// - Sets this engine to enabled allowing presentation to occur
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::Enable() noexcept
{
    _isEnabled = true;
    return S_OK;
}

// Routine Description:
// - Sets this engine to disabled to prevent presentation from occurring
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::Disable() noexcept
{
    _isEnabled = false;
    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the character region specified.
// - NOTE: This typically triggers on cursor or text buffer changes
// Arguments:
// - psrRegion - Character region (til::rect) that has been changed
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT UiaEngine::Invalidate(const til::rect* const /*psrRegion*/) noexcept
{
    _textBufferChanged = true;
    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the position of the cursor.
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - psrRegion - the region covered by the cursor
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::InvalidateCursor(const til::rect* const psrRegion) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, psrRegion);

    // check if cursor moved
    if (*psrRegion != _prevCursorRegion)
    {
        _prevCursorRegion = *psrRegion;
        _cursorChanged = true;
    }
    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - Invalidates a rectangle describing a pixel area on the display
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - prcDirtyClient - pixel rectangle
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::InvalidateSystem(const til::rect* const /*prcDirtyClient*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Notifies us that the console has changed the selection region and would
//      like it updated
// Arguments:
// - rectangles - One or more rectangles describing character positions on the grid
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept
{
    // early exit: different number of rows
    if (_prevSelection.size() != rectangles.size())
    {
        try
        {
            _selectionChanged = true;
            _prevSelection = rectangles;
        }
        CATCH_LOG_RETURN_HR(E_FAIL);
        return S_OK;
    }

    for (size_t i = 0; i < rectangles.size(); i++)
    {
        try
        {
            const auto prevRect = _prevSelection.at(i);
            const auto newRect = rectangles.at(i);

            // if any value is different, selection has changed
            if (prevRect.top != newRect.top || prevRect.right != newRect.right || prevRect.left != newRect.left || prevRect.bottom != newRect.bottom)
            {
                _selectionChanged = true;
                _prevSelection = rectangles;
                return S_OK;
            }
        }
        CATCH_LOG_RETURN_HR(E_FAIL);
    }

    // assume selection has not changed
    _selectionChanged = false;
    return S_OK;
}

// Routine Description:
// - Scrolls the existing dirty region (if it exists) and
//   invalidates the area that is uncovered in the window.
// Arguments:
// - pcoordDelta - The number of characters to move and uncover.
//               - -Y is up, Y is down, -X is left, X is right.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::InvalidateScroll(const til::point* const /*pcoordDelta*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Notifies to repaint everything.
// - NOTE: Use sparingly. Only use when something that could affect the entire
//      frame simultaneously occurs.
// Arguments:
// - <none>
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT UiaEngine::InvalidateAll() noexcept
{
    _textBufferChanged = true;
    return S_OK;
}

[[nodiscard]] HRESULT UiaEngine::NotifyNewText(const std::wstring_view newText) noexcept
try
{
    if (!newText.empty())
    {
        _newOutput.append(newText);
        _newOutput.push_back(L'\n');
        _textBufferChanged = true;
    }
    return S_OK;
}
CATCH_LOG_RETURN_HR(E_FAIL);

// Routine Description:
// - This is unused by this renderer.
// Arguments:
// - pForcePaint - always filled with false.
// Return Value:
// - S_FALSE because this is unused.
[[nodiscard]] HRESULT UiaEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);

    *pForcePaint = false;
    return S_FALSE;
}

// Routine Description:
// - Prepares internal structures for a painting operation.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we started to paint. S_FALSE if we didn't need to paint.
[[nodiscard]] HRESULT UiaEngine::StartPaint() noexcept
{
    RETURN_HR_IF(S_FALSE, !_isEnabled);

    // add more events here
    const auto somethingToDo = _selectionChanged || _textBufferChanged || _cursorChanged || !_queuedOutput.empty();

    // If there's nothing to do, quick return
    RETURN_HR_IF(S_FALSE, !somethingToDo);

    _isPainting = true;
    return S_OK;
}

// Routine Description:
// - Ends batch drawing and notifies automation clients of updated regions
// Arguments:
// - <none>
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT UiaEngine::EndPaint() noexcept
{
    RETURN_HR_IF(S_FALSE, !_isEnabled);
    RETURN_HR_IF(E_INVALIDARG, !_isPainting); // invalid to end paint when we're not painting

    // Snap this now while we're still under lock
    // so present can work on the copy while another
    // thread might start filling the next "frame"
    // worth of text data.
    std::swap(_queuedOutput, _newOutput);
    _newOutput.clear();
    return S_OK;
}

// RenderEngineBase defines a WaitUntilCanRender() that sleeps for 8ms to throttle rendering.
// But UiaEngine is never the only engine running. Overriding this function prevents
// us from sleeping 16ms per frame, when the other engine also sleeps for 8ms.
void UiaEngine::WaitUntilCanRender() noexcept
{
}

// Routine Description:
// - Used to perform longer running presentation steps outside the lock so the
//      other threads can continue.
// - Not currently used by UiaEngine.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE since we do nothing.
[[nodiscard]] HRESULT UiaEngine::Present() noexcept
{
    RETURN_HR_IF(S_FALSE, !_isEnabled);

    // Fire UIA Events here
    if (_selectionChanged)
    {
        try
        {
            _dispatcher->SignalSelectionChanged();
        }
        CATCH_LOG();
    }
    if (_textBufferChanged)
    {
        try
        {
            _dispatcher->SignalTextChanged();
        }
        CATCH_LOG();
    }
    if (_cursorChanged)
    {
        try
        {
            _dispatcher->SignalCursorChanged();
        }
        CATCH_LOG();
    }
    try
    {
        // The speech API is limited to 1000 characters at a time.
        // Break up the output into 1000 character chunks to ensure
        // the output isn't cut off.
        static constexpr size_t sapiLimit{ 1000 };
        const std::wstring_view output{ _queuedOutput };
        for (size_t offset = 0; offset < output.size(); offset += sapiLimit)
        {
            _dispatcher->NotifyNewOutput(output.substr(offset, sapiLimit));
        }
    }
    CATCH_LOG();

    _selectionChanged = false;
    _textBufferChanged = false;
    _cursorChanged = false;
    _isPainting = false;
    _queuedOutput.clear();

    return S_OK;
}

// Routine Description:
// - This is currently unused.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::ScrollFrame() noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Paints the background of the invalid area of the frame.
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE since we do nothing
[[nodiscard]] HRESULT UiaEngine::PaintBackground() noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Places one line of text onto the screen at the given position
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - clusters - Iterable collection of cluster information (text and columns it should consume)
// - coord - Character coordinate position in the cell grid
// - fTrimLeft - Whether or not to trim off the left half of a double wide character
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::PaintBufferLine(const std::span<const Cluster> /*clusters*/,
                                                 const til::point /*coord*/,
                                                 const bool /*trimLeft*/,
                                                 const bool /*lineWrapped*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Paints lines around cells (draws in pieces of the grid)
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - lines - <unused>
// - color - <unused>
// - cchLine - <unused>
// - coordTarget - <unused>
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::PaintBufferGridLines(GridLineSet const /*lines*/,
                                                      COLORREF const /*color*/,
                                                      size_t const /*cchLine*/,
                                                      const til::point /*coordTarget*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
//  - Reads the selected area, selection mode, and active screen buffer
//    from the global properties and dispatches a GDI invert on the selected text area.
//  Because the selection is the responsibility of the terminal, and not the
//      host, render nothing.
// Arguments:
//  - rect - Rectangle to invert or highlight to make the selection area
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::PaintSelection(const til::rect& /*rect*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Draws the cursor on the screen
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - options - Packed options relevant to how to draw the cursor
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::PaintCursor(const CursorOptions& /*options*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Updates the default brush colors used for drawing
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - textAttributes - <unused>
// - renderSettings - <unused>
// - pData - <unused>
// - usingSoftFont - <unused>
// - isSettingDefaultBrushes - <unused>
// Return Value:
// - S_FALSE since we do nothing
[[nodiscard]] HRESULT UiaEngine::UpdateDrawingBrushes(const TextAttribute& /*textAttributes*/,
                                                      const RenderSettings& /*renderSettings*/,
                                                      const gsl::not_null<IRenderData*> /*pData*/,
                                                      const bool /*usingSoftFont*/,
                                                      const bool /*isSettingDefaultBrushes*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Updates the font used for drawing
// Arguments:
// - pfiFontInfoDesired - <unused>
// - fiFontInfo - <unused>
// Return Value:
// - S_FALSE since we do nothing
[[nodiscard]] HRESULT UiaEngine::UpdateFont(const FontInfoDesired& /*pfiFontInfoDesired*/, FontInfo& /*fiFontInfo*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Sets the DPI in this renderer
// - Not currently used by UiaEngine.
// Arguments:
// - iDpi - DPI
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::UpdateDpi(const int /*iDpi*/) noexcept
{
    return S_FALSE;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT UiaEngine::UpdateViewport(const til::inclusive_rect& /*srNewViewport*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Currently unused by this renderer
// Arguments:
// - pfiFontInfoDesired - <unused>
// - pfiFontInfo - <unused>
// - iDpi - <unused>
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::GetProposedFont(const FontInfoDesired& /*pfiFontInfoDesired*/,
                                                 FontInfo& /*pfiFontInfo*/,
                                                 const int /*iDpi*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Gets the area that we currently believe is dirty within the character cell grid
// - Not currently used by UiaEngine.
// Arguments:
// - area - Rectangle describing dirty area in characters.
// Return Value:
// - S_OK.
[[nodiscard]] HRESULT UiaEngine::GetDirtyArea(std::span<const til::rect>& area) noexcept
{
    // Magic static is only valid because any instance of this object has the same behavior.
    // Use member variable instead if this ever changes.
    static constexpr til::rect empty;
    area = { &empty, 1 };
    return S_OK;
}

// Routine Description:
// - Gets the current font size
// Arguments:
// - pFontSize - Filled with the font size.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::GetFontSize(_Out_ til::size* const /*pFontSize*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Currently unused by this renderer.
// Arguments:
// - glyph - The glyph run to process for column width.
// - pResult - True if it should take two columns. False if it should take one.
// Return Value:
// - S_OK or relevant DirectWrite error.
[[nodiscard]] HRESULT UiaEngine::IsGlyphWideByFont(const std::wstring_view /*glyph*/, _Out_ bool* const /*pResult*/) noexcept
{
    return S_FALSE;
}

// Method Description:
// - Updates the window's title string.
// - Currently unused by this renderer.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::_DoUpdateTitle(_In_ const std::wstring_view /*newTitle*/) noexcept
{
    return S_FALSE;
}
