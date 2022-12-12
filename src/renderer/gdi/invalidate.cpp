// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "gdirenderer.hpp"
#include "../../types/inc/Viewport.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

// Routine Description:
// - Notifies us that the system has requested a particular pixel area of the client rectangle should be redrawn. (On WM_PAINT)
// Arguments:
// - prcDirtyClient - Pointer to pixel area (til::rect) of client region the system believes is dirty
// Return Value:
// - HRESULT S_OK, GDI-based error code, or safemath error
HRESULT GdiEngine::InvalidateSystem(const til::rect* const prcDirtyClient) noexcept
{
    RETURN_HR(_InvalidCombine(prcDirtyClient));
}

// Routine Description:
// - Notifies us that the console is attempting to scroll the existing screen area
// Arguments:
// - pcoordDelta - Pointer to character dimension (til::point) of the distance the console would like us to move while scrolling.
// Return Value:
// - HRESULT S_OK, GDI-based error code, or safemath error
HRESULT GdiEngine::InvalidateScroll(const til::point* const pcoordDelta) noexcept
{
    if (pcoordDelta->x != 0 || pcoordDelta->y != 0)
    {
        const auto ptDelta = *pcoordDelta * _GetFontSize();
        RETURN_IF_FAILED(_InvalidOffset(&ptDelta));
        _szInvalidScroll = _szInvalidScroll + ptDelta;
    }

    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the selection region and would like it updated
// Arguments:
// - rectangles - Vector of rectangles to draw, line by line
// Return Value:
// - HRESULT S_OK or GDI-based error code
HRESULT GdiEngine::InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept
{
    for (const auto& rect : rectangles)
    {
        RETURN_IF_FAILED(Invalidate(&rect));
    }

    return S_OK;
}

// Routine Description:
// - Notifies us that the console has changed the character region specified.
// - NOTE: This typically triggers on cursor or text buffer changes
// Arguments:
// - psrRegion - Character region (til::rect) that has been changed
// Return Value:
// - S_OK, GDI related failure, or safemath failure.
HRESULT GdiEngine::Invalidate(const til::rect* const psrRegion) noexcept
{
    const auto rcRegion = psrRegion->scale_up(_GetFontSize());
    RETURN_HR(_InvalidateRect(&rcRegion));
}

// Routine Description:
// - Notifies us that the console has changed the position of the cursor.
// Arguments:
// - psrRegion - the region covered by the cursor
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
HRESULT GdiEngine::InvalidateCursor(const til::rect* const psrRegion) noexcept
{
    return this->Invalidate(psrRegion);
}

// Routine Description:
// - Notifies to repaint everything.
// - NOTE: Use sparingly. Only use when something that could affect the entire frame simultaneously occurs.
// Arguments:
// - <none>
// Return Value:
// - S_OK, S_FALSE (if no window yet), GDI related failure, or safemath failure.
HRESULT GdiEngine::InvalidateAll() noexcept
{
    // If we don't have a window, don't bother.
    if (!_IsWindowValid())
    {
        return S_FALSE;
    }

    til::rect rc;
    RETURN_HR_IF(E_FAIL, !(GetClientRect(_hwndTargetWindow, rc.as_win32_rect())));
    RETURN_HR(InvalidateSystem(&rc));
}

// Method Description:
// - Notifies us that we're about to be torn down. This gives us a last chance
//      to force a repaint before the buffer contents are lost. The GDI renderer
//      doesn't care if we lose text - we're only painting visible text anyways,
//      so we return false.
// Arguments:
// - Receives a bool indicating if we should force the repaint.
// Return Value:
// - S_FALSE - we succeeded, but the result was false.
HRESULT GdiEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    *pForcePaint = false;
    return S_FALSE;
}

// Routine Description:
// - Helper to combine the given rectangle into the invalid region to be updated on the next paint
// Arguments:
// - prc - Pixel region (til::rect) that should be repainted on the next frame
// Return Value:
// - S_OK, GDI related failure, or safemath failure.
HRESULT GdiEngine::_InvalidCombine(const til::rect* const prc) noexcept
{
    if (!_fInvalidRectUsed)
    {
        _rcInvalid = *prc;
        _fInvalidRectUsed = true;
    }
    else
    {
        _OrRect(&_rcInvalid, prc);
    }

    // Ensure invalid areas remain within bounds of window.
    RETURN_IF_FAILED(_InvalidRestrict());

    return S_OK;
}

// Routine Description:
// - Helper to adjust the invalid region by the given offset such as when a scroll operation occurs.
// Arguments:
// - ppt - Distances by which we should move the invalid region in response to a scroll
// Return Value:
// - S_OK, GDI related failure, or safemath failure.
HRESULT GdiEngine::_InvalidOffset(const til::point* ppt) noexcept
{
    if (_fInvalidRectUsed)
    {
        til::rect rcInvalidNew;
        rcInvalidNew.left = _rcInvalid.left + ppt->x;
        rcInvalidNew.right = _rcInvalid.right + ppt->x;
        rcInvalidNew.top = _rcInvalid.top + ppt->y;
        rcInvalidNew.bottom = _rcInvalid.bottom + ppt->y;

        // Add the scrolled invalid rectangle to what was left behind to get the new invalid area.
        // This is the equivalent of adding in the "update rectangle" that we would get out of ScrollWindowEx/ScrollDC.
        _rcInvalid |= rcInvalidNew;

        // Ensure invalid areas remain within bounds of window.
        RETURN_IF_FAILED(_InvalidRestrict());
    }

    return S_OK;
}

// Routine Description:
// - Helper to ensure the invalid region remains within the bounds of the window.
// Arguments:
// - <none>
// Return Value:
// - S_OK, GDI related failure, or safemath failure.
HRESULT GdiEngine::_InvalidRestrict() noexcept
{
    // Ensure that the invalid area remains within the bounds of the client area
    til::rect rcClient;

    // Do restriction only if retrieving the client rect was successful.
    RETURN_HR_IF(E_FAIL, !(GetClientRect(_hwndTargetWindow, rcClient.as_win32_rect())));

    _rcInvalid.left = rcClient.left;
    _rcInvalid.right = rcClient.right;
    _rcInvalid.top = std::clamp(_rcInvalid.top, rcClient.top, rcClient.bottom);
    _rcInvalid.bottom = std::clamp(_rcInvalid.bottom, rcClient.top, rcClient.bottom);

    return S_OK;
}

// Routine Description:
// - Helper to add a pixel rectangle to the invalid area
// Arguments:
// - prc - Pointer to pixel rectangle representing invalid area to add to next paint frame
// Return Value:
// - S_OK, GDI related failure, or safemath failure.
HRESULT GdiEngine::_InvalidateRect(const til::rect* const prc) noexcept
{
    RETURN_HR(_InvalidCombine(prc));
}
