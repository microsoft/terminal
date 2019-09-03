// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "cursor.h"
#include "TextBuffer.hpp"

#pragma hdrstop

// Routine Description:
// - Constructor to set default properties for Cursor
// Arguments:
// - ulSize - The height of the cursor within this buffer
Cursor::Cursor(const ULONG ulSize, TextBuffer& parentBuffer) noexcept :
    _parentBuffer{ parentBuffer },
    _cPosition{ 0 },
    _fHasMoved(false),
    _fIsVisible(true),
    _fIsOn(true),
    _fIsDouble(false),
    _fBlinkingAllowed(true),
    _fDelay(false),
    _fIsConversionArea(false),
    _fIsPopupShown(false),
    _fDelayedEolWrap(false),
    _coordDelayedAt{ 0 },
    _fDeferCursorRedraw(false),
    _fHaveDeferredCursorRedraw(false),
    _ulSize(ulSize),
    _cursorType(CursorType::Legacy),
    _fUseColor(false),
    _color(s_InvertCursorColor)
{
}

Cursor::~Cursor()
{
}

COORD Cursor::GetPosition() const noexcept
{
    return _cPosition;
}

bool Cursor::HasMoved() const noexcept
{
    return _fHasMoved;
}

bool Cursor::IsVisible() const noexcept
{
    return _fIsVisible;
}

bool Cursor::IsOn() const noexcept
{
    return _fIsOn;
}

bool Cursor::IsBlinkingAllowed() const noexcept
{
    return _fBlinkingAllowed;
}

bool Cursor::IsDouble() const noexcept
{
    return _fIsDouble;
}

bool Cursor::IsConversionArea() const noexcept
{
    return _fIsConversionArea;
}

bool Cursor::IsPopupShown() const noexcept
{
    return _fIsPopupShown;
}

bool Cursor::GetDelay() const noexcept
{
    return _fDelay;
}

ULONG Cursor::GetSize() const noexcept
{
    return _ulSize;
}

void Cursor::SetHasMoved(const bool fHasMoved) noexcept
{
    _fHasMoved = fHasMoved;
}

void Cursor::SetIsVisible(const bool fIsVisible) noexcept
{
    _fIsVisible = fIsVisible;
    _RedrawCursor();
}

void Cursor::SetIsOn(const bool fIsOn) noexcept
{
    _fIsOn = fIsOn;
    _RedrawCursorAlways();
}

void Cursor::SetBlinkingAllowed(const bool fBlinkingAllowed) noexcept
{
    _fBlinkingAllowed = fBlinkingAllowed;
    _RedrawCursorAlways();
}

void Cursor::SetIsDouble(const bool fIsDouble) noexcept
{
    _fIsDouble = fIsDouble;
    _RedrawCursor();
}

void Cursor::SetIsConversionArea(const bool fIsConversionArea) noexcept
{
    // Functionally the same as "Hide cursor"
    // Never called with TRUE, it's only used in the creation of a
    //      ConversionAreaInfo, and never changed after that.
    _fIsConversionArea = fIsConversionArea;
    _RedrawCursorAlways();
}

void Cursor::SetIsPopupShown(const bool fIsPopupShown) noexcept
{
    // Functionally the same as "Hide cursor"
    _fIsPopupShown = fIsPopupShown;
    _RedrawCursorAlways();
}

void Cursor::SetDelay(const bool fDelay) noexcept
{
    _fDelay = fDelay;
}

void Cursor::SetSize(const ULONG ulSize) noexcept
{
    _ulSize = ulSize;
    _RedrawCursor();
}

void Cursor::SetStyle(const ULONG ulSize, const COLORREF color, const CursorType type) noexcept
{
    _ulSize = ulSize;
    _color = color;
    _cursorType = type;

    _RedrawCursor();
}

// Routine Description:
// - Sends a redraw message to the renderer only if the cursor is currently on.
// - NOTE: For use with most methods in this class.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Cursor::_RedrawCursor() noexcept
{
    // Only trigger the redraw if we're on.
    // Don't draw the cursor if this was triggered from a conversion area.
    // (Conversion areas have cursors to mark the insertion point internally, but the user's actual cursor is the one on the primary screen buffer.)
    if (IsOn() && !IsConversionArea())
    {
        if (_fDeferCursorRedraw)
        {
            _fHaveDeferredCursorRedraw = true;
        }
        else
        {
            _RedrawCursorAlways();
        }
    }
}

// Routine Description:
// - Sends a redraw message to the renderer no matter what.
// - NOTE: For use with the method that turns the cursor on and off to force a refresh
//         and clear the ON cursor from the screen. Not for use with other methods.
//         They should use the other method so refreshes are suppressed while the cursor is off.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Cursor::_RedrawCursorAlways() noexcept
{
    try
    {
        _parentBuffer.GetRenderTarget().TriggerRedrawCursor(&_cPosition);
    }
    CATCH_LOG();
}

void Cursor::SetPosition(const COORD cPosition) noexcept
{
    _RedrawCursor();
    _cPosition.X = cPosition.X;
    _cPosition.Y = cPosition.Y;
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::SetXPosition(const int NewX) noexcept
{
    _RedrawCursor();
    _cPosition.X = gsl::narrow<SHORT>(NewX);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::SetYPosition(const int NewY) noexcept
{
    _RedrawCursor();
    _cPosition.Y = gsl::narrow<SHORT>(NewY);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::IncrementXPosition(const int DeltaX) noexcept
{
    _RedrawCursor();
    _cPosition.X += gsl::narrow<SHORT>(DeltaX);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::IncrementYPosition(const int DeltaY) noexcept
{
    _RedrawCursor();
    _cPosition.Y += gsl::narrow<SHORT>(DeltaY);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::DecrementXPosition(const int DeltaX) noexcept
{
    _RedrawCursor();
    _cPosition.X -= gsl::narrow<SHORT>(DeltaX);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

void Cursor::DecrementYPosition(const int DeltaY) noexcept
{
    _RedrawCursor();
    _cPosition.Y -= gsl::narrow<SHORT>(DeltaY);
    _RedrawCursor();
    ResetDelayEOLWrap();
}

///////////////////////////////////////////////////////////////////////////////
// Routine Description:
// - Copies properties from another cursor into this one.
// - This is primarily to copy properties that would otherwise not be specified during CreateInstance
// - NOTE: As of now, this function is specifically used to handle the ResizeWithReflow operation.
//         It will need modification for other future users.
// Arguments:
// - OtherCursor - The cursor to copy properties from
// Return Value:
// - <none>
void Cursor::CopyProperties(const Cursor& OtherCursor) noexcept
{
    // We shouldn't copy the position as it will be already rearranged by the resize operation.
    //_cPosition                    = pOtherCursor->_cPosition;

    _fHasMoved = OtherCursor._fHasMoved;
    _fIsVisible = OtherCursor._fIsVisible;
    _fIsOn = OtherCursor._fIsOn;
    _fIsDouble = OtherCursor._fIsDouble;
    _fBlinkingAllowed = OtherCursor._fBlinkingAllowed;
    _fDelay = OtherCursor._fDelay;
    _fIsConversionArea = OtherCursor._fIsConversionArea;

    // A resize operation should invalidate the delayed end of line status, so do not copy.
    //_fDelayedEolWrap              = OtherCursor._fDelayedEolWrap;
    //_coordDelayedAt               = OtherCursor._coordDelayedAt;

    _fDeferCursorRedraw = OtherCursor._fDeferCursorRedraw;
    _fHaveDeferredCursorRedraw = OtherCursor._fHaveDeferredCursorRedraw;

    // Size will be handled seperately in the resize operation.
    //_ulSize                       = OtherCursor._ulSize;
    _cursorType = OtherCursor._cursorType;
    _color = OtherCursor._color;
}

void Cursor::DelayEOLWrap(const COORD coordDelayedAt) noexcept
{
    _coordDelayedAt = coordDelayedAt;
    _fDelayedEolWrap = true;
}

void Cursor::ResetDelayEOLWrap() noexcept
{
    _coordDelayedAt = { 0 };
    _fDelayedEolWrap = false;
}

COORD Cursor::GetDelayedAtPosition() const noexcept
{
    return _coordDelayedAt;
}

bool Cursor::IsDelayedEOLWrap() const noexcept
{
    return _fDelayedEolWrap;
}

void Cursor::StartDeferDrawing() noexcept
{
    _fDeferCursorRedraw = true;
}

void Cursor::EndDeferDrawing() noexcept
{
    if (_fHaveDeferredCursorRedraw)
    {
        _RedrawCursorAlways();
    }

    _fDeferCursorRedraw = FALSE;
}

const CursorType Cursor::GetType() const noexcept
{
    return _cursorType;
}

const bool Cursor::IsUsingColor() const noexcept
{
    return GetColor() != INVALID_COLOR;
}

const COLORREF Cursor::GetColor() const noexcept
{
    return _color;
}

void Cursor::SetColor(const unsigned int color) noexcept
{
    _color = gsl::narrow_cast<COLORREF>(color);
}

void Cursor::SetType(const CursorType type) noexcept
{
    _cursorType = type;
}
