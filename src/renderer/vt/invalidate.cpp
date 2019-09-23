// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "vtrenderer.hpp"

using namespace Microsoft::Console::Types;
#pragma hdrstop

using namespace Microsoft::Console::Render;

// Routine Description:
// - Notifies us that the system has requested a particular pixel area of the
//      client rectangle should be redrawn. (On WM_PAINT)
//  For VT, this doesn't mean anything. So do nothing.
// Arguments:
// - prcDirtyClient - Pointer to pixel area (RECT) of client region the system
//      believes is dirty
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::InvalidateSystem(const RECT* const /*prcDirtyClient*/) noexcept
{
    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the selection region and would
//      like it updated
// Arguments:
// - rectangles - Vector of rectangles to draw, line by line
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::InvalidateSelection(const std::vector<SMALL_RECT>& /*rectangles*/) noexcept
{
    // Selection shouldn't be handled bt the VT Renderer Host, it should be
    //      handled by the client.

    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the character region specified.
// - NOTE: This typically triggers on cursor or text buffer changes
// Arguments:
// - psrRegion - Character region (SMALL_RECT) that has been changed
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::Invalidate(const SMALL_RECT* const psrRegion) noexcept
{
    Viewport newInvalid = Viewport::FromExclusive(*psrRegion);
    _trace.TraceInvalidate(newInvalid);

    return this->_InvalidCombine(newInvalid);
}

// Routine Description:
// - Notifies us that the console has changed the position of the cursor.
// Arguments:
// - pcoordCursor - the new position of the cursor
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::InvalidateCursor(const COORD* const pcoordCursor) noexcept
{
    // If we just inherited the cursor, we're going to get an InvalidateCursor
    //      for both where the old cursor was, and where the new cursor is
    //      (the inherited location). (See Cursor.cpp:Cursor::SetPosition)
    // We should ignore the first one, but after that, if the client application
    //      is moving the cursor around in the viewport, move our virtual top
    //      up to meet their changes.
    if (!_skipCursor && _virtualTop > pcoordCursor->Y)
    {
        _virtualTop = pcoordCursor->Y;
    }
    _skipCursor = false;

    _cursorMoved = true;
    return S_OK;
}

// Routine Description:
// - Notifies to repaint everything.
// - NOTE: Use sparingly. Only use when something that could affect the entire
//      frame simultaneously occurs.
// Arguments:
// - <none>
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::InvalidateAll() noexcept
{
    _trace.TraceInvalidateAll(_lastViewport.ToOrigin());
    return this->_InvalidCombine(_lastViewport.ToOrigin());
}

// Method Description:
// - Notifies us that we're about to circle the buffer, giving us a chance to
//      force a repaint before the buffer contents are lost. The VT renderer
//      needs to be able to render all text before it's lost, so we return true.
// Arguments:
// - Recieves a bool indicating if we should force the repaint.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::InvalidateCircling(_Out_ bool* const pForcePaint) noexcept
{
    // If we're in the middle of a resize request, don't try to immediately start a frame.
    if (_inResizeRequest)
    {
        *pForcePaint = false;
    }
    else
    {
        *pForcePaint = true;

        // Keep track of the fact that we circled, we'll need to do some work on
        //      end paint to specifically handle this.
        _circled = true;
    }

    return S_OK;
}

// Method Description:
// - Notifies us that we're about to be torn down. This gives us a last chance
//      to force a repaint before the buffer contents are lost. The VT renderer
//      needs to be able to render all text before it's lost, so we return true.
// Arguments:
// - Recieves a bool indicating if we should force the repaint.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    *pForcePaint = true;
    return S_OK;
}

// Routine Description:
// - Helper to combine the given rectangle into the invalid region to be
//      updated on the next paint
// Expects EXCLUSIVE rectangles.
// Arguments:
// - invalid - A viewport containing the character region that should be
//      repainted on the next frame
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_InvalidCombine(const Viewport invalid) noexcept
{
    if (!_fInvalidRectUsed)
    {
        _invalidRect = invalid;
        _fInvalidRectUsed = true;
    }
    else
    {
        _invalidRect = Viewport::Union(_invalidRect, invalid);
    }

    // Ensure invalid areas remain within bounds of window.
    RETURN_IF_FAILED(_InvalidRestrict());

    return S_OK;
}

// Routine Description:
// - Helper to adjust the invalid region by the given offset such as when a
//      scroll operation occurs.
// Arguments:
// - ppt - Distances by which we should move the invalid region in response to a scroll
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_InvalidOffset(const COORD* const pCoord) noexcept
{
    if (_fInvalidRectUsed)
    {
        try
        {
            Viewport newInvalid = Viewport::Offset(_invalidRect, *pCoord);

            // Add the scrolled invalid rectangle to what was left behind to get the new invalid area.
            // This is the equivalent of adding in the "update rectangle" that we would get out of ScrollWindowEx/ScrollDC.
            _invalidRect = Viewport::Union(_invalidRect, newInvalid);

            // Ensure invalid areas remain within bounds of window.
            RETURN_IF_FAILED(_InvalidRestrict());
        }
        CATCH_RETURN();
    }

    return S_OK;
}

// Routine Description:
// - Helper to ensure the invalid region remains within the bounds of the viewport.
// Arguments:
// - <none>
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or safemath failure.
[[nodiscard]] HRESULT VtEngine::_InvalidRestrict() noexcept
{
    SMALL_RECT oldInvalid = _invalidRect.ToExclusive();

    _lastViewport.ToOrigin().TrimToViewport(&oldInvalid);

    _invalidRect = Viewport::FromExclusive(oldInvalid);

    return S_OK;
}
