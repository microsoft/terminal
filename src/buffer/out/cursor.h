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

    // No Copy. It will copy the timer handle. Bad news.
    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) & = delete;

    Cursor(Cursor&&) = default;
    Cursor& operator=(Cursor&&) & = delete;

    uint64_t GetLastMutationId() const noexcept;
    bool IsVisible() const noexcept;
    bool IsBlinking() const noexcept;
    bool IsDouble() const noexcept;
    ULONG GetSize() const noexcept;
    til::point GetPosition() const noexcept;
    CursorType GetType() const noexcept;

    void SetIsVisible(bool enable) noexcept;
    void SetIsBlinking(bool enable) noexcept;
    void SetIsDouble(const bool fIsDouble) noexcept;
    void SetSize(const ULONG ulSize) noexcept;
    void SetStyle(const ULONG ulSize, const CursorType type) noexcept;

    void SetPosition(const til::point cPosition) noexcept;
    void SetXPosition(const til::CoordType NewX) noexcept;
    void SetYPosition(const til::CoordType NewY) noexcept;
    void IncrementXPosition(const til::CoordType DeltaX) noexcept;
    void IncrementYPosition(const til::CoordType DeltaY) noexcept;
    void DecrementXPosition(const til::CoordType DeltaX) noexcept;
    void DecrementYPosition(const til::CoordType DeltaY) noexcept;

    void CopyProperties(const Cursor& other) noexcept;

    void DelayEOLWrap() noexcept;
    void ResetDelayEOLWrap() noexcept;
    const std::optional<til::point>& GetDelayEOLWrap() const noexcept;

    void SetType(const CursorType type) noexcept;

private:
    void _redrawIfVisible() noexcept;
    void _redraw() noexcept;

    TextBuffer& _parentBuffer;

    //TODO: separate the rendering and text placement

    // NOTE: If you are adding a property here, go add it to CopyProperties.

    uint64_t _mutationId = 0;
    til::point _cPosition; // current position on screen (in screen buffer coords).
    std::optional<til::point> _coordDelayedAt; // coordinate the EOL wrap was delayed at.
    ULONG _ulSize;
    CursorType _cursorType = CursorType::Legacy;
    bool _isVisible = true;
    bool _isBlinking = true;
    bool _fIsDouble = false; // whether the cursor size should be doubled
};
