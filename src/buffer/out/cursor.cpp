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
    _ulSize(ulSize)
{
}

Cursor::~Cursor() = default;

til::point Cursor::GetPosition() const noexcept
{
    return _cPosition;
}

bool Cursor::IsVisible() const noexcept
{
    return _isVisible;
}

bool Cursor::IsBlinking() const noexcept
{
    return _isBlinking;
}

bool Cursor::IsDouble() const noexcept
{
    return _fIsDouble;
}

ULONG Cursor::GetSize() const noexcept
{
    return _ulSize;
}

void Cursor::SetIsVisible(bool enable)
{
    if (_isVisible != enable)
    {
        _isVisible = enable;
        _redrawIfVisible();
    }
}

void Cursor::SetIsBlinking(bool enable)
{
    if (_isBlinking != enable)
    {
        _isBlinking = enable;
        _redrawIfVisible();
    }
}

void Cursor::SetIsDouble(const bool fIsDouble) noexcept
{
    if (_fIsDouble != fIsDouble)
    {
        _fIsDouble = fIsDouble;
        _redrawIfVisible();
    }
}

void Cursor::SetSize(const ULONG ulSize) noexcept
{
    if (_ulSize != ulSize)
    {
        _ulSize = ulSize;
        _redrawIfVisible();
    }
}

void Cursor::SetStyle(const ULONG ulSize, const CursorType type) noexcept
{
    if (_ulSize != ulSize || _cursorType != type)
    {
        _ulSize = ulSize;
        _cursorType = type;
        _redrawIfVisible();
    }
}

void Cursor::SetPosition(const til::point cPosition) noexcept
{
    if (_cPosition != cPosition)
    {
        _cPosition = cPosition;
        _redrawIfVisible();
    }
}

void Cursor::SetXPosition(const til::CoordType NewX) noexcept
{
    SetPosition({ NewX, _cPosition.y });
}

void Cursor::SetYPosition(const til::CoordType NewY) noexcept
{
    SetPosition({ _cPosition.x, NewY });
}

void Cursor::IncrementXPosition(const til::CoordType DeltaX) noexcept
{
    SetPosition({ _cPosition.x + DeltaX, _cPosition.y });
}

void Cursor::IncrementYPosition(const til::CoordType DeltaY) noexcept
{
    SetPosition({ _cPosition.x, _cPosition.y + DeltaY });
}

void Cursor::DecrementXPosition(const til::CoordType DeltaX) noexcept
{
    SetPosition({ _cPosition.x - DeltaX, _cPosition.y });
}

void Cursor::DecrementYPosition(const til::CoordType DeltaY) noexcept
{
    SetPosition({ _cPosition.x, _cPosition.y - DeltaY });
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
void Cursor::CopyProperties(const Cursor& other) noexcept
{
    _cPosition = other._cPosition;
    _coordDelayedAt = other._coordDelayedAt;
    _ulSize = other._ulSize;
    _cursorType = other._cursorType;
    _isVisible = other._isVisible;
    _isBlinking = other._isBlinking;
    _fIsDouble = other._fIsDouble;
}

void Cursor::DelayEOLWrap() noexcept
{
    _coordDelayedAt = _cPosition;
}

void Cursor::ResetDelayEOLWrap() noexcept
{
    _coordDelayedAt.reset();
}

const std::optional<til::point>& Cursor::GetDelayEOLWrap() const noexcept
{
    return _coordDelayedAt;
}

CursorType Cursor::GetType() const noexcept
{
    return _cursorType;
}

void Cursor::SetType(const CursorType type) noexcept
{
    _cursorType = type;
}

void Cursor::_redrawIfVisible() const noexcept
{
    if (IsVisible())
    {
        _redraw();
    }
}

void Cursor::_redraw() const noexcept
{
    _parentBuffer.NotifyPaintFrame();
}
