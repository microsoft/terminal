/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- cursor.h

Abstract:
- This file implements the NT console server cursor routines.

Author:
- Therese Stowell (ThereseS) 5-Dec-1990

Revision History:
- Grouped into class and items made private. (MiNiksa, 2014)
--*/

#pragma once

#include "../inc/conattrs.hpp"

class TextBuffer;

class Cursor final
{
public:
    // the following values are used to create the textmode cursor.
    static constexpr unsigned int CURSOR_SMALL_SIZE = 25; // large enough to be one pixel on a six pixel font

    Cursor(const ULONG ulSize, TextBuffer& parentBuffer) noexcept;

    ~Cursor();

    // No Copy. It will copy the timer handle. Bad news.
    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) & = delete;

    Cursor(Cursor&&) = default;
    Cursor& operator=(Cursor&&) & = delete;

    bool HasMoved() const noexcept;
    bool IsVisible() const noexcept;
    bool IsOn() const noexcept;
    bool IsBlinkingAllowed() const noexcept;
    bool IsDouble() const noexcept;
    bool IsConversionArea() const noexcept;
    bool IsPopupShown() const noexcept;
    bool GetDelay() const noexcept;
    ULONG GetSize() const noexcept;
    til::point GetPosition() const noexcept;

    const CursorType GetType() const noexcept;

    void StartDeferDrawing() noexcept;
    bool IsDeferDrawing() noexcept;
    void EndDeferDrawing() noexcept;

    void SetHasMoved(const bool fHasMoved) noexcept;
    void SetIsVisible(const bool fIsVisible) noexcept;
    void SetIsOn(const bool fIsOn) noexcept;
    void SetBlinkingAllowed(const bool fIsOn) noexcept;
    void SetIsDouble(const bool fIsDouble) noexcept;
    void SetIsConversionArea(const bool fIsConversionArea) noexcept;
    void SetIsPopupShown(const bool fIsPopupShown) noexcept;
    void SetDelay(const bool fDelay) noexcept;
    void SetSize(const ULONG ulSize) noexcept;
    void SetStyle(const ULONG ulSize, const CursorType type) noexcept;

    void SetPosition(const til::point cPosition) noexcept;
    void SetXPosition(const til::CoordType NewX) noexcept;
    void SetYPosition(const til::CoordType NewY) noexcept;
    void IncrementXPosition(const til::CoordType DeltaX) noexcept;
    void IncrementYPosition(const til::CoordType DeltaY) noexcept;
    void DecrementXPosition(const til::CoordType DeltaX) noexcept;
    void DecrementYPosition(const til::CoordType DeltaY) noexcept;

    void CopyProperties(const Cursor& OtherCursor) noexcept;

    void DelayEOLWrap() noexcept;
    void ResetDelayEOLWrap() noexcept;
    til::point GetDelayedAtPosition() const noexcept;
    bool IsDelayedEOLWrap() const noexcept;

    void SetType(const CursorType type) noexcept;

private:
    TextBuffer& _parentBuffer;

    //TODO: separate the rendering and text placement

    // NOTE: If you are adding a property here, go add it to CopyProperties.

    til::point _cPosition; // current position on screen (in screen buffer coords).

    bool _fHasMoved;
    bool _fIsVisible; // whether cursor is visible (set only through the API)
    bool _fIsOn; // whether blinking cursor is on or not
    bool _fIsDouble; // whether the cursor size should be doubled
    bool _fBlinkingAllowed; //Whether or not the cursor is allowed to blink at all. only set through VT (^[[?12h/l)
    bool _fDelay; // don't blink scursor on next timer message
    bool _fIsConversionArea; // is attached to a conversion area so it doesn't actually need to display the cursor.
    bool _fIsPopupShown; // if a popup is being shown, turn off, stop blinking.

    bool _fDelayedEolWrap; // don't wrap at EOL till the next char comes in.
    til::point _coordDelayedAt; // coordinate the EOL wrap was delayed at.

    bool _fDeferCursorRedraw; // whether we should defer redrawing the cursor or not
    bool _fHaveDeferredCursorRedraw; // have we been asked to redraw the cursor while it was being deferred?

    ULONG _ulSize;

    void _RedrawCursor() noexcept;
    void _RedrawCursorAlways() noexcept;

    CursorType _cursorType;
};
