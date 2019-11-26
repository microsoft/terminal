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
UiaEngine::UiaEngine() noexcept :
    _isPainting{ false },
    _isEnabled{ false },
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
// - Sets this engine to disabled to prevent presentation from occuring
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
// - psrRegion - Character region (SMALL_RECT) that has been changed
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT UiaEngine::Invalidate(const SMALL_RECT* const /*psrRegion*/) noexcept
{
    return E_NOTIMPL;
}

// Routine Description:
// - Notifies us that the console has changed the position of the cursor.
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - pcoordCursor - the new position of the cursor
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::InvalidateCursor(const COORD* const /*pcoordCursor*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Invalidates a rectangle describing a pixel area on the display
//  For UIA, this doesn't mean anything. So do nothing.
// Arguments:
// - prcDirtyClient - pixel rectangle
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::InvalidateSystem(const RECT* const /*prcDirtyClient*/) noexcept
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
[[nodiscard]] HRESULT UiaEngine::InvalidateSelection(const std::vector<SMALL_RECT>& /*rectangles*/) noexcept
{
    return E_NOTIMPL;
}

// Routine Description:
// - Scrolls the existing dirty region (if it exists) and
//   invalidates the area that is uncovered in the window.
// Arguments:
// - pcoordDelta - The number of characters to move and uncover.
//               - -Y is up, Y is down, -X is left, X is right.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::InvalidateScroll(const COORD* const /*pcoordDelta*/) noexcept
{
    return E_NOTIMPL;
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
    return E_NOTIMPL;
}

// Routine Description:
// - This currently has no effect in this renderer.
// Arguments:
// - pForcePaint - Always filled with false
// Return Value:
// - S_FALSE because we don't use this.
[[nodiscard]] HRESULT UiaEngine::InvalidateCircling(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);

    *pForcePaint = false;
    return S_FALSE;
}

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

    // If there's nothing to do, quick return
    bool somethingToDo = false;

    if (_isEnabled)
    {
        somethingToDo = false;

        if (somethingToDo)
        {
            _isPainting = true;
        }
    }

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
    RETURN_HR_IF(E_INVALIDARG, !_isPainting); // invalid to end paint when we're not painting

    if (_isEnabled)
    {
        _isPainting = false;

        // fire UIA events here
    }

    return S_OK;
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
    return S_FALSE;
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
// Arguments:
// - clusters - Iterable collection of cluster information (text and columns it should consume)
// - coord - Character coordinate position in the cell grid
// - fTrimLeft - Whether or not to trim off the left half of a double wide character
// Return Value:
// - E_NOTIMPL
[[nodiscard]] HRESULT UiaEngine::PaintBufferLine(std::basic_string_view<Cluster> const /*clusters*/,
                                                 COORD const /*coord*/,
                                                 const bool /*trimLeft*/) noexcept
{
    return E_NOTIMPL;
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
[[nodiscard]] HRESULT UiaEngine::PaintBufferGridLines(GridLines const /*lines*/,
                                                      COLORREF const /*color*/,
                                                      size_t const /*cchLine*/,
                                                      COORD const /*coordTarget*/) noexcept
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
[[nodiscard]] HRESULT UiaEngine::PaintSelection(const SMALL_RECT /*rect*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Draws the cursor on the screen
// Arguments:
// - options - Packed options relevant to how to draw the cursor
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::PaintCursor(const IRenderEngine::CursorOptions& /*options*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Updates the default brush colors used for drawing
// - Not currently used by UiaEngine.
// Arguments:
// - colorForeground - <unused>
// - colorBackground - <unused>
// - legacyColorAttribute - <unused>
// - isBold - <unused>
// - isSettingDefaultBrushes - <unused>
// Return Value:
// - S_FALSE since we do nothing
[[nodiscard]] HRESULT UiaEngine::UpdateDrawingBrushes(const COLORREF /*colorForeground*/,
                                                      const COLORREF /*colorBackground*/,
                                                      const WORD /*legacyColorAttribute*/,
                                                      const ExtendedAttributes /*extendedAttrs*/,
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
    return E_NOTIMPL;
}

// Routine Description:
// - Sets the DPI in this renderer
// - Not currently used by UiaEngine.
// Arguments:
// - iDpi - DPI
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::UpdateDpi(int const /*iDpi*/) noexcept
{
    return E_NOTIMPL;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT UiaEngine::UpdateViewport(const SMALL_RECT /*srNewViewport*/) noexcept
{
    return E_NOTIMPL;
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
                                                 int const /*iDpi*/) noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Gets the area that we currently believe is dirty within the character cell grid
// - Not currently used by UiaEngine.
// Arguments:
// - <none>
// Return Value:
// - Rectangle describing dirty area in characters.
[[nodiscard]] SMALL_RECT UiaEngine::GetDirtyRectInChars() noexcept
{
    return Viewport::Empty().ToInclusive();
}

// Routine Description:
// - Gets the current font size
// Arguments:
// - pFontSize - Filled with the font size.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT UiaEngine::GetFontSize(_Out_ COORD* const /*pFontSize*/) noexcept
{
    return E_NOTIMPL;
}

// Routine Description:
// - Currently unused by this renderer.
// Arguments:
// - glyph - The glyph run to process for column width.
// - pResult - True if it should take two columns. False if it should take one.
// Return Value:
// - E_NOTIMPL
[[nodiscard]] HRESULT UiaEngine::IsGlyphWideByFont(const std::wstring_view /*glyph*/, _Out_ bool* const /*pResult*/) noexcept
{
    return E_NOTIMPL;
}

// Method Description:
// - Updates the window's title string.
// - Currently unused by this renderer.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_FALSE
[[nodiscard]] HRESULT UiaEngine::_DoUpdateTitle(_In_ const std::wstring& /*newTitle*/) noexcept
{
    return S_FALSE;
}
