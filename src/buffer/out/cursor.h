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
    static constexpr UINT CURSOR_SMALL_SIZE = 25; // large enough to be one pixel on a six pixel font

    Cursor(UINT ulSize, TextBuffer& parentBuffer) noexcept;

    bool HasMoved() const noexcept;
    bool IsVisible() const noexcept;
    bool IsOn() const noexcept;
    bool IsBlinkingAllowed() const noexcept;
    bool IsDouble() const noexcept;
    bool IsConversionArea() const noexcept;
    bool IsPopupShown() const noexcept;
    bool GetDelay() const noexcept;
    UINT GetSize() const noexcept;
    til::point GetPosition() const noexcept;

    const CursorType GetType() const noexcept;

    void StartDeferDrawing() noexcept;
    bool IsDeferDrawing() noexcept;
    void EndDeferDrawing() noexcept;

    void SetHasMoved(bool fHasMoved) noexcept;
    void SetIsVisible(bool fIsVisible) noexcept;
    void SetIsOn(bool fIsOn) noexcept;
    void SetBlinkingAllowed(bool fIsOn) noexcept;
    void SetIsDouble(bool fIsDouble) noexcept;
    void SetIsConversionArea(bool fIsConversionArea) noexcept;
    void SetIsPopupShown(bool fIsPopupShown) noexcept;
    void SetDelay(bool fDelay) noexcept;
    void SetSize(UINT ulSize) noexcept;
    void SetStyle(UINT ulSize, CursorType type) noexcept;

    void SetPosition(til::point cPosition) noexcept;
    void SetXPosition(til::CoordType NewX) noexcept;
    void SetYPosition(til::CoordType NewY) noexcept;
    void IncrementXPosition(til::CoordType DeltaX) noexcept;
    void IncrementYPosition(til::CoordType DeltaY) noexcept;
    void DecrementXPosition(til::CoordType DeltaX) noexcept;
    void DecrementYPosition(til::CoordType DeltaY) noexcept;

    void CopyProperties(const Cursor& OtherCursor) noexcept;

    void DelayEOLWrap(til::point coordDelayedAt) noexcept;
    void ResetDelayEOLWrap() noexcept;
    til::point GetDelayedAtPosition() const noexcept;
    bool IsDelayedEOLWrap() const noexcept;

    void SetType(CursorType type) noexcept;

private:
    TextBuffer& _parentBuffer;

    //TODO: separate the rendering and text placement

    // NOTE: If you are adding a property here, go add it to CopyProperties.

    til::point _cPosition; // current position on screen (in screen buffer coords).

    bool _fHasMoved = false;
    bool _fIsVisible = true; // whether cursor is visible (set only through the API)
    bool _fIsOn = true; // whether blinking cursor is on or not
    bool _fIsDouble = false; // whether the cursor size should be doubled
    bool _fBlinkingAllowed = true; //Whether or not the cursor is allowed to blink at all. only set through VT (^[[?12h/l)
    bool _fDelay = false; // don't blink scursor on next timer message
    bool _fIsConversionArea = false; // is attached to a conversion area so it doesn't actually need to display the cursor.
    bool _fIsPopupShown = false; // if a popup is being shown, turn off, stop blinking.

    bool _fDelayedEolWrap = false; // don't wrap at EOL till the next char comes in.
    til::point _coordDelayedAt; // coordinate the EOL wrap was delayed at.

    bool _fDeferCursorRedraw = false; // whether we should defer redrawing the cursor or not
    bool _fHaveDeferredCursorRedraw = false; // have we been asked to redraw the cursor while it was being deferred?

    UINT _ulSize = CURSOR_SMALL_SIZE;

    void _RedrawCursor() noexcept;
    void _RedrawCursorAlways() noexcept;

    CursorType _cursorType = CursorType::Legacy;
};
