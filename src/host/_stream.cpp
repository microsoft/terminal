// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ApiRoutines.h"

#include "_stream.h"
#include "stream.h"
#include "writeData.hpp"

#include "_output.h"
#include "output.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "utf8ToWidecharParser.hpp"

#include "../types/inc/convert.hpp"
#include "../types/inc/GlyphWidth.hpp"
#include "../types/inc/Viewport.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop
using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::StateMachine;
// Used by WriteCharsLegacy.
#define IS_GLYPH_CHAR(wch) (((wch) < L' ') || ((wch) == 0x007F))

constexpr unsigned int LOCAL_BUFFER_SIZE = 100;

// Routine Description:
// - This routine updates the cursor position.  Its input is the non-special
//   cased new location of the cursor.  For example, if the cursor were being
//   moved one space backwards from the left edge of the screen, the X
//   coordinate would be -1.  This routine would set the X coordinate to
//   the right edge of the screen and decrement the Y coordinate by one.
// Arguments:
// - screenInfo - reference to screen buffer information structure.
// - coordCursor - New location of cursor.
// - fKeepCursorVisible - TRUE if changing window origin desirable when hit right edge
// Return Value:
[[nodiscard]] NTSTATUS AdjustCursorPosition(SCREEN_INFORMATION& screenInfo,
                                            _In_ COORD coordCursor,
                                            const BOOL fKeepCursorVisible,
                                            _Inout_opt_ PSHORT psScrollY)
{
    const COORD bufferSize = screenInfo.GetBufferSize().Dimensions();
    if (coordCursor.X < 0)
    {
        if (coordCursor.Y > 0)
        {
            coordCursor.X = (SHORT)(bufferSize.X + coordCursor.X);
            coordCursor.Y = (SHORT)(coordCursor.Y - 1);
        }
        else
        {
            coordCursor.X = 0;
        }
    }
    else if (coordCursor.X >= bufferSize.X)
    {
        // at end of line. if wrap mode, wrap cursor.  otherwise leave it where it is.
        if (screenInfo.OutputMode & ENABLE_WRAP_AT_EOL_OUTPUT)
        {
            coordCursor.Y += coordCursor.X / bufferSize.X;
            coordCursor.X = coordCursor.X % bufferSize.X;
        }
        else
        {
            coordCursor.X = screenInfo.GetTextBuffer().GetCursor().GetPosition().X;
        }
    }

    const auto bufferAttributes = screenInfo.GetAttributes();

    const auto relativeMargins = screenInfo.GetRelativeScrollMargins();
    auto viewport = screenInfo.GetViewport();
    SMALL_RECT srMargins = screenInfo.GetAbsoluteScrollMargins().ToInclusive();
    const bool fMarginsSet = srMargins.Bottom > srMargins.Top;
    COORD currentCursor = screenInfo.GetTextBuffer().GetCursor().GetPosition();
    const int iCurrentCursorY = currentCursor.Y;
    const bool inVtMode = WI_IsFlagSet(screenInfo.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    const bool fCursorInMargins = iCurrentCursorY <= srMargins.Bottom && iCurrentCursorY >= srMargins.Top;
    const bool cursorAboveViewport = coordCursor.Y < 0 && inVtMode;
    const bool fScrollDown = fMarginsSet && fCursorInMargins && (coordCursor.Y > srMargins.Bottom);
    bool fScrollUp = fMarginsSet && fCursorInMargins && (coordCursor.Y < srMargins.Top);

    const bool fScrollUpWithoutMargins = (!fMarginsSet) && cursorAboveViewport;
    // if we're in VT mode, AND MARGINS AREN'T SET and a Reverse Line Feed took the cursor up past the top of the viewport,
    //   VT style scroll the contents of the screen.
    // This can happen in applications like `less`, that don't set margins, because they're going to
    //   scroll the entire screen anyways, so no need for them to ever set the margins.
    if (fScrollUpWithoutMargins)
    {
        fScrollUp = true;
        srMargins.Top = 0;
        srMargins.Bottom = screenInfo.GetViewport().BottomInclusive();
    }

    const bool scrollDownAtTop = fScrollDown && relativeMargins.Top() == 0;
    if (scrollDownAtTop)
    {
        // We're trying to scroll down, and the top margin is at the top of the viewport.
        // In this case, we want the lines that are "scrolled off" to appear in
        //      the scrollback instead of being discarded.
        // To do this, we're going to scroll everything starting at the bottom
        //  margin down, then move the viewport down.

        const SHORT delta = coordCursor.Y - srMargins.Bottom;
        SMALL_RECT scrollRect{ 0 };
        scrollRect.Left = 0;
        scrollRect.Top = srMargins.Bottom + 1; // One below margins
        scrollRect.Bottom = bufferSize.Y - 1; // -1, otherwise this would be an exclusive rect.
        scrollRect.Right = bufferSize.X - 1; // -1, otherwise this would be an exclusive rect.

        // This is the Y position we're moving the contents below the bottom margin to.
        SHORT moveToYPosition = scrollRect.Top + delta;

        // This is where the viewport will need to be to give the effect of
        //      scrolling the contents in the margins.
        SHORT newViewTop = viewport.Top() + delta;

        // This is how many new lines need to be added to the buffer to support this operation.
        const SHORT newRows = (viewport.BottomExclusive() + delta) - bufferSize.Y;

        // If we're near the bottom of the buffer, we might need to insert some
        //      new rows at the bottom.
        // If we do this, then the viewport is now one line higher than it used
        //      to be, so it needs to move down by one less line.
        for (auto i = 0; i < newRows; i++)
        {
            screenInfo.GetTextBuffer().IncrementCircularBuffer();
            moveToYPosition--;
            newViewTop--;
            scrollRect.Top--;
        }

        const COORD newPostMarginsOrigin = { 0, moveToYPosition };
        const COORD newViewOrigin = { 0, newViewTop };

        try
        {
            ScrollRegion(screenInfo, scrollRect, std::nullopt, newPostMarginsOrigin, UNICODE_SPACE, bufferAttributes);
        }
        CATCH_LOG();

        // Move the viewport down
        auto hr = screenInfo.SetViewportOrigin(true, newViewOrigin, true);
        if (FAILED(hr))
        {
            return NTSTATUS_FROM_HRESULT(hr);
        }
        // If we didn't actually move the viewport, it's because we're at the
        //      bottom of the buffer, and the top lines of the viewport have
        //      changed. Manually invalidate here, to make sure the screen
        //      displays the correct text.
        if (newViewOrigin == viewport.Origin())
        {
            Viewport invalid = Viewport::FromDimensions(viewport.Origin(), { viewport.Width(), delta });
            screenInfo.GetRenderTarget().TriggerRedraw(invalid);
        }

        // reset where our local viewport is, and recalculate the cursor and
        //      margin positions.
        viewport = screenInfo.GetViewport();
        if (newRows > 0)
        {
            currentCursor.Y -= newRows;
            coordCursor.Y -= newRows;
        }
        srMargins = screenInfo.GetAbsoluteScrollMargins().ToInclusive();
    }

    // If we did the above scrollDownAtTop case, then we've already scrolled
    //      the margins content, and we can skip this.
    if (fScrollUp || (fScrollDown && !scrollDownAtTop))
    {
        SHORT diff = coordCursor.Y - (fScrollUp ? srMargins.Top : srMargins.Bottom);

        SMALL_RECT scrollRect = { 0 };
        scrollRect.Top = srMargins.Top;
        scrollRect.Bottom = srMargins.Bottom;
        scrollRect.Left = 0; // NOTE: Left/Right Scroll margins don't do anything currently.
        scrollRect.Right = bufferSize.X - 1; // -1, otherwise this would be an exclusive rect.

        COORD dest;
        dest.X = scrollRect.Left;
        dest.Y = scrollRect.Top - diff;

        try
        {
            ScrollRegion(screenInfo, scrollRect, scrollRect, dest, UNICODE_SPACE, bufferAttributes);
        }
        CATCH_LOG();

        coordCursor.Y -= diff;
    }

    NTSTATUS Status = STATUS_SUCCESS;

    if (coordCursor.Y >= bufferSize.Y)
    {
        // At the end of the buffer. Scroll contents of screen buffer so new position is visible.
        FAIL_FAST_IF(!(coordCursor.Y == bufferSize.Y));
        if (!StreamScrollRegion(screenInfo))
        {
            Status = STATUS_NO_MEMORY;
        }

        if (nullptr != psScrollY)
        {
            *psScrollY += (SHORT)(bufferSize.Y - coordCursor.Y - 1);
        }
        coordCursor.Y += (SHORT)(bufferSize.Y - coordCursor.Y - 1);
    }

    const bool cursorMovedPastViewport = coordCursor.Y > screenInfo.GetViewport().BottomInclusive();
    const bool cursorMovedPastVirtualViewport = coordCursor.Y > screenInfo.GetVirtualViewport().BottomInclusive();
    if (NT_SUCCESS(Status))
    {
        // if at right or bottom edge of window, scroll right or down one char.
        if (cursorMovedPastViewport)
        {
            COORD WindowOrigin;
            WindowOrigin.X = 0;
            WindowOrigin.Y = coordCursor.Y - screenInfo.GetViewport().BottomInclusive();
            Status = screenInfo.SetViewportOrigin(false, WindowOrigin, true);
        }
    }

    if (NT_SUCCESS(Status))
    {
        if (fKeepCursorVisible)
        {
            screenInfo.MakeCursorVisible(coordCursor);
        }
        Status = screenInfo.SetCursorPosition(coordCursor, !!fKeepCursorVisible);

        // MSFT:19989333 - Only re-initialize the cursor row if the cursor moved
        //      below the terminal section of the buffer (the virtual viewport),
        //      and the visible part of the buffer (the actual viewport).
        // If this is only cursorMovedPastViewport, and you scroll up, then type
        //      a character, we'll re-initialize the line the cursor is on.
        // If this is only cursorMovedPastVirtualViewport and you scroll down,
        //      (with terminal scrolling disabled) then all lines newly exposed
        //      will get their attributes constantly cleared out.
        // Both cursorMovedPastViewport and cursorMovedPastVirtualViewport works
        if (inVtMode && cursorMovedPastViewport && cursorMovedPastVirtualViewport)
        {
            screenInfo.InitializeCursorRowAttributes();
        }
    }

    return Status;
}

// Routine Description:
// - This routine writes a string to the screen, processing any embedded
//   unicode characters.  The string is also copied to the input buffer, if
//   the output mode is line mode.
// Arguments:
// - screenInfo - reference to screen buffer information structure.
// - pwchBufferBackupLimit - Pointer to beginning of buffer.
// - pwchBuffer - Pointer to buffer to copy string to.  assumed to be at least as long as pwchRealUnicode.
//                This pointer is updated to point to the next position in the buffer.
// - pwchRealUnicode - Pointer to string to write.
// - pcb - On input, number of bytes to write.  On output, number of bytes written.
// - pcSpaces - On output, the number of spaces consumed by the written characters.
// - dwFlags -
//      WC_DESTRUCTIVE_BACKSPACE backspace overwrites characters.
//      WC_KEEP_CURSOR_VISIBLE   change window origin desirable when hit rt. edge
//      WC_ECHO                  if called by Read (echoing characters)
// Return Value:
// Note:
// - This routine does not process tabs and backspace properly.  That code will be implemented as part of the line editing services.
[[nodiscard]] NTSTATUS WriteCharsLegacy(SCREEN_INFORMATION& screenInfo,
                                        _In_range_(<=, pwchBuffer) const wchar_t* const pwchBufferBackupLimit,
                                        _In_ const wchar_t* pwchBuffer,
                                        _In_reads_bytes_(*pcb) const wchar_t* pwchRealUnicode,
                                        _Inout_ size_t* const pcb,
                                        _Out_opt_ size_t* const pcSpaces,
                                        const SHORT sOriginalXPosition,
                                        const DWORD dwFlags,
                                        _Inout_opt_ PSHORT const psScrollY)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    TextBuffer& textBuffer = screenInfo.GetTextBuffer();
    Cursor& cursor = textBuffer.GetCursor();
    COORD CursorPosition = cursor.GetPosition();
    NTSTATUS Status = STATUS_SUCCESS;
    SHORT XPosition;
    WCHAR LocalBuffer[LOCAL_BUFFER_SIZE];
    size_t TempNumSpaces = 0;
    const bool fUnprocessed = WI_IsFlagClear(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT);

    // Must not adjust cursor here. It has to stay on for many write scenarios. Consumers should call for the
    // cursor to be turned off if they want that.

    const TextAttribute Attributes = screenInfo.GetAttributes();
    const size_t BufferSize = *pcb;
    *pcb = 0;

    const wchar_t* lpString = pwchRealUnicode;

    const COORD coordScreenBufferSize = screenInfo.GetBufferSize().Dimensions();

    while (*pcb < BufferSize)
    {
        // correct for delayed EOL
        if (cursor.IsDelayedEOLWrap())
        {
            const COORD coordDelayedAt = cursor.GetDelayedAtPosition();
            cursor.ResetDelayEOLWrap();
            // Only act on a delayed EOL if we didn't move the cursor to a different position from where the EOL was marked.
            if (coordDelayedAt.X == CursorPosition.X && coordDelayedAt.Y == CursorPosition.Y)
            {
                bool fDoEolWrap = false;

                if (WI_IsFlagSet(dwFlags, WC_DELAY_EOL_WRAP))
                {
                    // Correct if it's a printable character and whoever called us still understands/wants delayed EOL wrap.
                    if (*lpString >= UNICODE_SPACE)
                    {
                        fDoEolWrap = true;
                    }
                    else if (*lpString == UNICODE_BACKSPACE)
                    {
                        // if we have an active wrap and a backspace comes in, process it by moving the cursor
                        // back one cell position unless it's already at the start of a row.
                        *pcb += sizeof(WCHAR);
                        lpString++;
                        pwchRealUnicode++;
                        if (CursorPosition.X != 0)
                        {
                            --CursorPosition.X;
                            Status = AdjustCursorPosition(screenInfo, CursorPosition, WI_IsFlagSet(dwFlags, WC_KEEP_CURSOR_VISIBLE), psScrollY);
                            CursorPosition = cursor.GetPosition();
                        }
                        continue;
                    }
                }
                else
                {
                    // Uh oh, we've hit a consumer that doesn't know about delayed end of lines. To rectify this, just quickly jump
                    // forward to the next line as if we had done it earlier, then let everything else play out normally.
                    fDoEolWrap = true;
                }

                if (fDoEolWrap)
                {
                    CursorPosition.X = 0;
                    CursorPosition.Y++;

                    Status = AdjustCursorPosition(screenInfo, CursorPosition, WI_IsFlagSet(dwFlags, WC_KEEP_CURSOR_VISIBLE), psScrollY);

                    CursorPosition = cursor.GetPosition();
                }
            }
        }

        if (screenInfo.InVTMode())
        {
            // if we're at the beginning of a row and we get a backspace and told to limit backspacing, skip it
            if (*lpString == UNICODE_BACKSPACE && CursorPosition.X == 0 && WI_IsFlagSet(dwFlags, WC_LIMIT_BACKSPACE))
            {
                *pcb += sizeof(wchar_t);
                ++lpString;
                ++pwchRealUnicode;
                continue;
            }
        }

        // As an optimization, collect characters in buffer and print out all at once.
        XPosition = cursor.GetPosition().X;
        size_t i = 0;
        wchar_t* LocalBufPtr = LocalBuffer;
        while (*pcb < BufferSize && i < LOCAL_BUFFER_SIZE && XPosition < coordScreenBufferSize.X)
        {
#pragma prefast(suppress : 26019, "Buffer is taken in multiples of 2. Validation is ok.")
            const wchar_t Char = *lpString;
            const wchar_t RealUnicodeChar = *pwchRealUnicode;
            if (!IS_GLYPH_CHAR(RealUnicodeChar) || fUnprocessed)
            {
                if (IsGlyphFullWidth(Char))
                {
                    if (i < (LOCAL_BUFFER_SIZE - 1) && XPosition < (coordScreenBufferSize.X - 1))
                    {
                        *LocalBufPtr++ = Char;

                        // cursor adjusted by 2 because the char is double width
                        XPosition += 2;
                        i += 1;
                        pwchBuffer++;
                    }
                    else
                    {
                        goto EndWhile;
                    }
                }
                else
                {
                    *LocalBufPtr = Char;
                    LocalBufPtr++;
                    XPosition++;
                    i++;
                    pwchBuffer++;
                }
            }
            else
            {
                FAIL_FAST_IF(!(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT)));
                switch (RealUnicodeChar)
                {
                case UNICODE_BELL:
                    if (dwFlags & WC_ECHO)
                    {
                        goto CtrlChar;
                    }
                    else
                    {
                        screenInfo.SendNotifyBeep();
                    }
                    break;
                case UNICODE_BACKSPACE:

                    // automatically go to EndWhile.  this is because
                    // backspace is not destructive, so "aBkSp" prints
                    // a with the cursor on the "a". we could achieve
                    // this behavior staying in this loop and figuring out
                    // the string that needs to be printed, but it would
                    // be expensive and it's the exceptional case.

                    goto EndWhile;
                    break;
                case UNICODE_TAB:
                    if (screenInfo.InVTMode())
                    {
                        goto EndWhile;
                    }
                    else
                    {
                        const ULONG TabSize = NUMBER_OF_SPACES_IN_TAB(XPosition);
                        XPosition = (SHORT)(XPosition + TabSize);
                        if (XPosition >= coordScreenBufferSize.X || WI_IsFlagSet(dwFlags, WC_NONDESTRUCTIVE_TAB))
                        {
                            goto EndWhile;
                        }

                        for (ULONG j = 0; j < TabSize && i < LOCAL_BUFFER_SIZE; j++, i++)
                        {
                            *LocalBufPtr = UNICODE_SPACE;
                            LocalBufPtr++;
                        }
                    }

                    pwchBuffer++;
                    break;
                case UNICODE_LINEFEED:
                case UNICODE_CARRIAGERETURN:
                    goto EndWhile;
                default:

                    // if char is ctrl char, write ^char.
                    if ((dwFlags & WC_ECHO) && (IS_CONTROL_CHAR(RealUnicodeChar)))
                    {
                    CtrlChar:
                        if (i < (LOCAL_BUFFER_SIZE - 1))
                        {
                            *LocalBufPtr = (WCHAR)'^';
                            LocalBufPtr++;
                            XPosition++;
                            i++;

                            *LocalBufPtr = (WCHAR)(RealUnicodeChar + (WCHAR)'@');
                            LocalBufPtr++;
                            XPosition++;
                            i++;

                            pwchBuffer++;
                        }
                        else
                        {
                            goto EndWhile;
                        }
                    }
                    else
                    {
                        if (Char == UNICODE_NULL)
                        {
                            *LocalBufPtr = UNICODE_SPACE;
                        }
                        else
                        {
                            // As a special favor to incompetent apps that attempt to display control chars,
                            // convert to corresponding OEM Glyph Chars
                            WORD CharType;

                            GetStringTypeW(CT_CTYPE1, &RealUnicodeChar, 1, &CharType);
                            if (WI_IsFlagSet(CharType, C1_CNTRL))
                            {
                                ConvertOutputToUnicode(gci.OutputCP,
                                                       (LPSTR)&RealUnicodeChar,
                                                       1,
                                                       LocalBufPtr,
                                                       1);
                            }
                            else
                            {
                                *LocalBufPtr = Char;
                            }
                        }

                        LocalBufPtr++;
                        XPosition++;
                        i++;
                        pwchBuffer++;
                    }
                }
            }
            lpString++;
            pwchRealUnicode++;
            *pcb += sizeof(WCHAR);
        }
    EndWhile:
        if (i != 0)
        {
            CursorPosition = cursor.GetPosition();

            // Make sure we don't write past the end of the buffer.
            if (i > (ULONG)coordScreenBufferSize.X - CursorPosition.X)
            {
                i = (ULONG)coordScreenBufferSize.X - CursorPosition.X;
            }

            // line was wrapped if we're writing up to the end of the current row
            OutputCellIterator it(std::wstring_view(LocalBuffer, i), Attributes);
            const auto itEnd = screenInfo.Write(it);

            // Notify accessibility
            screenInfo.NotifyAccessibilityEventing(CursorPosition.X, CursorPosition.Y, CursorPosition.X + gsl::narrow<SHORT>(i - 1), CursorPosition.Y);

            // The number of "spaces" or "cells" we have consumed needs to be reported and stored for later
            // when/if we need to erase the command line.
            TempNumSpaces += itEnd.GetCellDistance(it);
            CursorPosition.X = XPosition;

            // enforce a delayed newline if we're about to pass the end and the WC_DELAY_EOL_WRAP flag is set.
            if (WI_IsFlagSet(dwFlags, WC_DELAY_EOL_WRAP) && CursorPosition.X >= coordScreenBufferSize.X)
            {
                // Our cursor position as of this time is going to remain on the last position in this column.
                CursorPosition.X = coordScreenBufferSize.X - 1;

                // Update in the structures that we're still pointing to the last character in the row
                cursor.SetPosition(CursorPosition);

                // Record for the delay comparison that we're delaying on the last character in the row
                cursor.DelayEOLWrap(CursorPosition);
            }
            else
            {
                Status = AdjustCursorPosition(screenInfo, CursorPosition, WI_IsFlagSet(dwFlags, WC_KEEP_CURSOR_VISIBLE), psScrollY);
            }

            if (*pcb == BufferSize)
            {
                if (nullptr != pcSpaces)
                {
                    *pcSpaces = TempNumSpaces;
                }
                return STATUS_SUCCESS;
            }
            continue;
        }
        else if (*pcb >= BufferSize)
        {
            FAIL_FAST_IF(!(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT)));

            // this catches the case where the number of backspaces == the number of characters.
            if (nullptr != pcSpaces)
            {
                *pcSpaces = TempNumSpaces;
            }
            return STATUS_SUCCESS;
        }

        FAIL_FAST_IF(!(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT)));
        switch (*lpString)
        {
        case UNICODE_BACKSPACE:
        {
            // move cursor backwards one space. overwrite current char with blank.
            // we get here because we have to backspace from the beginning of the line
            TempNumSpaces -= 1;
            if (pwchBuffer == pwchBufferBackupLimit)
            {
                CursorPosition.X -= 1;
            }
            else
            {
                const wchar_t* Tmp;
                wchar_t* Tmp2 = nullptr;
                WCHAR LastChar;

                const size_t bufferSize = pwchBuffer - pwchBufferBackupLimit;
                std::unique_ptr<wchar_t[]> buffer;
                try
                {
                    buffer = std::make_unique<wchar_t[]>(bufferSize);
                    std::fill_n(buffer.get(), bufferSize, UNICODE_NULL);
                }
                catch (...)
                {
                    return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
                }

                for (i = 0, Tmp2 = buffer.get(), Tmp = pwchBufferBackupLimit;
                     i < bufferSize;
                     i++, Tmp++)
                {
                    // see 18120085, these two need to be seperate if statements
                    if (*Tmp == UNICODE_BACKSPACE)
                    {
                        //it is important we do nothing in the else case for
                        //      this one instead of falling through to the below else.
                        if (Tmp2 > buffer.get())
                        {
                            Tmp2--;
                        }
                    }
                    else
                    {
                        FAIL_FAST_IF(!(Tmp2 >= buffer.get()));
                        *Tmp2++ = *Tmp;
                    }
                }
                if (Tmp2 == buffer.get())
                {
                    LastChar = UNICODE_SPACE;
                }
                else
                {
#pragma prefast(suppress : 26001, "This is fine. Tmp2 has to have advanced or it would equal pBuffer.")
                    LastChar = *(Tmp2 - 1);
                }

                if (LastChar == UNICODE_TAB)
                {
                    CursorPosition.X -= (SHORT)(RetrieveNumberOfSpaces(sOriginalXPosition,
                                                                       pwchBufferBackupLimit,
                                                                       (ULONG)(pwchBuffer - pwchBufferBackupLimit - 1)));
                    if (CursorPosition.X < 0)
                    {
                        CursorPosition.X = (coordScreenBufferSize.X - 1) / TAB_SIZE;
                        CursorPosition.X *= TAB_SIZE;
                        CursorPosition.X += 1;
                        CursorPosition.Y -= 1;

                        // since you just backspaced yourself back up into the previous row, unset the wrap
                        // flag on the prev row if it was set
                        textBuffer.GetRowByOffset(CursorPosition.Y).GetCharRow().SetWrapForced(false);
                    }
                }
                else if (IS_CONTROL_CHAR(LastChar))
                {
                    CursorPosition.X -= 1;
                    TempNumSpaces -= 1;

                    // overwrite second character of ^x sequence.
                    if (dwFlags & WC_DESTRUCTIVE_BACKSPACE)
                    {
                        try
                        {
                            screenInfo.Write(OutputCellIterator(UNICODE_SPACE, Attributes, 1), CursorPosition);
                            Status = STATUS_SUCCESS;
                        }
                        CATCH_LOG();
                    }

                    CursorPosition.X -= 1;
                }
                else if (IsGlyphFullWidth(LastChar))
                {
                    CursorPosition.X -= 1;
                    TempNumSpaces -= 1;

                    Status = AdjustCursorPosition(screenInfo, CursorPosition, dwFlags & WC_KEEP_CURSOR_VISIBLE, psScrollY);
                    if (dwFlags & WC_DESTRUCTIVE_BACKSPACE)
                    {
                        try
                        {
                            screenInfo.Write(OutputCellIterator(UNICODE_SPACE, Attributes, 1), CursorPosition);
                            Status = STATUS_SUCCESS;
                        }
                        CATCH_LOG();
                    }
                    CursorPosition.X -= 1;
                }
                else
                {
                    CursorPosition.X--;
                }
            }
            if ((dwFlags & WC_LIMIT_BACKSPACE) && (CursorPosition.X < 0))
            {
                CursorPosition.X = 0;
                OutputDebugStringA(("CONSRV: Ignoring backspace to previous line\n"));
            }
            Status = AdjustCursorPosition(screenInfo, CursorPosition, (dwFlags & WC_KEEP_CURSOR_VISIBLE) != 0, psScrollY);
            if (dwFlags & WC_DESTRUCTIVE_BACKSPACE)
            {
                try
                {
                    screenInfo.Write(OutputCellIterator(UNICODE_SPACE, Attributes, 1), cursor.GetPosition());
                }
                CATCH_LOG();
            }
            if (cursor.GetPosition().X == 0 && (screenInfo.OutputMode & ENABLE_WRAP_AT_EOL_OUTPUT) && pwchBuffer > pwchBufferBackupLimit)
            {
                if (CheckBisectProcessW(screenInfo,
                                        pwchBufferBackupLimit,
                                        pwchBuffer + 1 - pwchBufferBackupLimit,
                                        coordScreenBufferSize.X - sOriginalXPosition,
                                        sOriginalXPosition,
                                        dwFlags & WC_ECHO))
                {
                    CursorPosition.X = coordScreenBufferSize.X - 1;
                    CursorPosition.Y = (SHORT)(cursor.GetPosition().Y - 1);

                    // since you just backspaced yourself back up into the previous row, unset the wrap flag
                    // on the prev row if it was set
                    textBuffer.GetRowByOffset(CursorPosition.Y).GetCharRow().SetWrapForced(false);

                    Status = AdjustCursorPosition(screenInfo, CursorPosition, dwFlags & WC_KEEP_CURSOR_VISIBLE, psScrollY);
                }
            }
            break;
        }
        case UNICODE_TAB:
        {
            // if VT-style tabs are set, then handle them the VT way, including not inserting spaces.
            // just move the cursor to the next tab stop.
            if (screenInfo.InVTMode())
            {
                const COORD cCursorOld = cursor.GetPosition();
                // Get Forward tab handles tabbing past the end of the buffer
                CursorPosition = screenInfo.GetForwardTab(cCursorOld);
            }
            else
            {
                const size_t TabSize = NUMBER_OF_SPACES_IN_TAB(cursor.GetPosition().X);
                CursorPosition.X = (SHORT)(cursor.GetPosition().X + TabSize);

                // move cursor forward to next tab stop.  fill space with blanks.
                // we get here when the tab extends beyond the right edge of the
                // window.  if the tab goes wraps the line, set the cursor to the first
                // position in the next line.
                pwchBuffer++;

                TempNumSpaces += TabSize;
                size_t NumChars = 0;
                if (CursorPosition.X >= coordScreenBufferSize.X)
                {
                    NumChars = gsl::narrow<size_t>(coordScreenBufferSize.X - cursor.GetPosition().X);
                    CursorPosition.X = 0;
                    CursorPosition.Y = cursor.GetPosition().Y + 1;

                    // since you just tabbed yourself past the end of the row, set the wrap
                    textBuffer.GetRowByOffset(cursor.GetPosition().Y).GetCharRow().SetWrapForced(true);
                }
                else
                {
                    NumChars = gsl::narrow<size_t>(CursorPosition.X - cursor.GetPosition().X);
                    CursorPosition.Y = cursor.GetPosition().Y;
                }

                if (!WI_IsFlagSet(dwFlags, WC_NONDESTRUCTIVE_TAB))
                {
                    try
                    {
                        const OutputCellIterator it(UNICODE_SPACE, Attributes, NumChars);
                        const auto done = screenInfo.Write(it, cursor.GetPosition());
                        NumChars = done.GetCellDistance(it);
                    }
                    CATCH_LOG();
                }
            }
            Status = AdjustCursorPosition(screenInfo, CursorPosition, (dwFlags & WC_KEEP_CURSOR_VISIBLE) != 0, psScrollY);
            break;
        }
        case UNICODE_CARRIAGERETURN:
        {
            // Carriage return moves the cursor to the beginning of the line.
            // We don't need to worry about handling cr or lf for
            // backspace because input is sent to the user on cr or lf.
            pwchBuffer++;
            CursorPosition.X = 0;
            CursorPosition.Y = cursor.GetPosition().Y;
            Status = AdjustCursorPosition(screenInfo, CursorPosition, (dwFlags & WC_KEEP_CURSOR_VISIBLE) != 0, psScrollY);
            break;
        }
        case UNICODE_LINEFEED:
        {
            // move cursor to the next line.
            pwchBuffer++;

            if (gci.IsReturnOnNewlineAutomatic())
            {
                // Traditionally, we reset the X position to 0 with a newline automatically.
                // Some things might not want this automatic "ONLCR line discipline" (for example, things that are expecting a *NIX behavior.)
                // They will turn it off with an output mode flag.
                CursorPosition.X = 0;
            }

            CursorPosition.Y = (SHORT)(cursor.GetPosition().Y + 1);

            {
                // since we explicitly just moved down a row, clear the wrap status on the row we just came from
                textBuffer.GetRowByOffset(cursor.GetPosition().Y).GetCharRow().SetWrapForced(false);
            }

            Status = AdjustCursorPosition(screenInfo, CursorPosition, (dwFlags & WC_KEEP_CURSOR_VISIBLE) != 0, psScrollY);
            break;
        }
        default:
        {
            const wchar_t Char = *lpString;
            if (Char >= UNICODE_SPACE &&
                IsGlyphFullWidth(Char) &&
                XPosition >= (coordScreenBufferSize.X - 1) &&
                (screenInfo.OutputMode & ENABLE_WRAP_AT_EOL_OUTPUT))
            {
                const COORD TargetPoint = cursor.GetPosition();
                ROW& Row = textBuffer.GetRowByOffset(TargetPoint.Y);
                CharRow& charRow = Row.GetCharRow();

                try
                {
                    // If we're on top of a trailing cell, clear it and the previous cell.
                    if (charRow.DbcsAttrAt(TargetPoint.X).IsTrailing())
                    {
                        // Space to clear for 2 cells.
                        OutputCellIterator it(UNICODE_SPACE, 2);

                        // Back target point up one.
                        auto writeTarget = TargetPoint;
                        writeTarget.X--;

                        // Write 2 clear cells.
                        screenInfo.Write(it, writeTarget);
                    }
                }
                catch (...)
                {
                    return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
                }

                CursorPosition.X = 0;
                CursorPosition.Y = (SHORT)(TargetPoint.Y + 1);

                // since you just moved yourself down onto the next row with 1 character, that sounds like a
                // forced wrap so set the flag
                charRow.SetWrapForced(true);

                // Additionally, this padding is only called for IsConsoleFullWidth (a.k.a. when a character
                // is too wide to fit on the current line).
                charRow.SetDoubleBytePadded(true);

                Status = AdjustCursorPosition(screenInfo, CursorPosition, dwFlags & WC_KEEP_CURSOR_VISIBLE, psScrollY);
                continue;
            }
            break;
        }
        }
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }

        *pcb += sizeof(WCHAR);
        lpString++;
        pwchRealUnicode++;
    }

    if (nullptr != pcSpaces)
    {
        *pcSpaces = TempNumSpaces;
    }

    return STATUS_SUCCESS;
}

// Routine Description:
// - This routine writes a string to the screen, processing any embedded
//   unicode characters.  The string is also copied to the input buffer, if
//   the output mode is line mode.
// Arguments:
// - screenInfo - reference to screen buffer information structure.
// - pwchBufferBackupLimit - Pointer to beginning of buffer.
// - pwchBuffer - Pointer to buffer to copy string to.  assumed to be at least as long as pwchRealUnicode.
//              This pointer is updated to point to the next position in the buffer.
// - pwchRealUnicode - Pointer to string to write.
// - pcb - On input, number of bytes to write.  On output, number of bytes written.
// - pcSpaces - On output, the number of spaces consumed by the written characters.
// - dwFlags -
//      WC_DESTRUCTIVE_BACKSPACE backspace overwrites characters.
//      WC_KEEP_CURSOR_VISIBLE   change window origin (viewport) desirable when hit rt. edge
//      WC_ECHO                  if called by Read (echoing characters)
// Return Value:
// Note:
// - This routine does not process tabs and backspace properly.  That code will be implemented as part of the line editing services.
[[nodiscard]] NTSTATUS WriteChars(SCREEN_INFORMATION& screenInfo,
                                  _In_range_(<=, pwchBuffer) const wchar_t* const pwchBufferBackupLimit,
                                  _In_ const wchar_t* pwchBuffer,
                                  _In_reads_bytes_(*pcb) const wchar_t* pwchRealUnicode,
                                  _Inout_ size_t* const pcb,
                                  _Out_opt_ size_t* const pcSpaces,
                                  const SHORT sOriginalXPosition,
                                  const DWORD dwFlags,
                                  _Inout_opt_ PSHORT const psScrollY)
{
    if (!WI_IsFlagSet(screenInfo.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) ||
        !WI_IsFlagSet(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT))
    {
        return WriteCharsLegacy(screenInfo,
                                pwchBufferBackupLimit,
                                pwchBuffer,
                                pwchRealUnicode,
                                pcb,
                                pcSpaces,
                                sOriginalXPosition,
                                dwFlags,
                                psScrollY);
    }

    NTSTATUS Status = STATUS_SUCCESS;

    size_t const BufferSize = *pcb;
    *pcb = 0;

    {
        size_t TempNumSpaces = 0;

        {
            if (NT_SUCCESS(Status))
            {
                FAIL_FAST_IF(!(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT)));
                FAIL_FAST_IF(!(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING)));

                // defined down in the WriteBuffer default case hiding on the other end of the state machine. See outputStream.cpp
                // This is the only mode used by DoWriteConsole.
                FAIL_FAST_IF(!(WI_IsFlagSet(dwFlags, WC_LIMIT_BACKSPACE)));

                StateMachine& machine = screenInfo.GetStateMachine();
                size_t const cch = BufferSize / sizeof(WCHAR);

                machine.ProcessString(pwchRealUnicode, cch);
                *pcb += BufferSize;
            }
        }

        if (nullptr != pcSpaces)
        {
            *pcSpaces = TempNumSpaces;
        }
    }

    return Status;
}

// Routine Description:
// - Takes the given text and inserts it into the given screen buffer.
// Note:
// - Console lock must be held when calling this routine
// - String has been translated to unicode at this point.
// Arguments:
// - pwchBuffer - wide character text to be inserted into buffer
// - pcbBuffer - byte count of pwchBuffer on the way in, number of bytes consumed on the way out.
// - screenInfo - Screen Information class to write the text into at the current cursor position
// - ppWaiter - If writing to the console is blocked for whatever reason, this will be filled with a pointer to context
//              that can be used by the server to resume the call at a later time.
// Return Value:
// - STATUS_SUCCESS if OK.
// - CONSOLE_STATUS_WAIT if we couldn't finish now and need to be called back later (see ppWaiter).
// - Or a suitable NTSTATUS format error code for memory/string/math failures.
[[nodiscard]] NTSTATUS DoWriteConsole(_In_reads_bytes_(*pcbBuffer) PWCHAR pwchBuffer,
                                      _Inout_ size_t* const pcbBuffer,
                                      SCREEN_INFORMATION& screenInfo,
                                      std::unique_ptr<WriteData>& waiter)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (WI_IsAnyFlagSet(gci.Flags, (CONSOLE_SUSPENDED | CONSOLE_SELECTING | CONSOLE_SCROLLBAR_TRACKING)))
    {
        try
        {
            waiter = std::make_unique<WriteData>(screenInfo,
                                                 pwchBuffer,
                                                 *pcbBuffer,
                                                 gci.OutputCP);
        }
        catch (...)
        {
            return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        return CONSOLE_STATUS_WAIT;
    }

    const auto& textBuffer = screenInfo.GetTextBuffer();
    return WriteChars(screenInfo,
                      pwchBuffer,
                      pwchBuffer,
                      pwchBuffer,
                      pcbBuffer,
                      nullptr,
                      textBuffer.GetCursor().GetPosition().X,
                      WC_LIMIT_BACKSPACE,
                      nullptr);
}

// Routine Description:
// - This method performs the actual work of attempting to write to the console, converting data types as necessary
//   to adapt from the server types to the legacy internal host types.
// - It operates on Unicode data only. It's assumed the text is translated by this point.
// Arguments:
// - OutContext - the console output object to write the new text into
// - pwsTextBuffer - wide character text buffer provided by client application to insert
// - cchTextBufferLength - text buffer counted in characters
// - pcchTextBufferRead - character count of the number of characters we were able to insert before returning
// - ppWaiter - If we are blocked from writing now and need to wait, this is filled with contextual data for the server to restore the call later
// Return Value:
// - S_OK if successful.
// - S_OK if we need to wait (check if ppWaiter is not nullptr).
// - Or a suitable HRESULT code for math/string/memory failures.
[[nodiscard]] HRESULT WriteConsoleWImplHelper(IConsoleOutputObject& context,
                                              const std::wstring_view buffer,
                                              size_t& read,
                                              std::unique_ptr<WriteData>& waiter) noexcept
{
    try
    {
        // Set out variables in case we exit early.
        read = 0;
        waiter.reset();

        // Convert characters to bytes to give to DoWriteConsole.
        size_t cbTextBufferLength;
        RETURN_IF_FAILED(SizeTMult(buffer.size(), sizeof(wchar_t), &cbTextBufferLength));

        NTSTATUS Status = DoWriteConsole(const_cast<wchar_t*>(buffer.data()), &cbTextBufferLength, context, waiter);

        // Convert back from bytes to characters for the resulting string length written.
        read = cbTextBufferLength / sizeof(wchar_t);

        if (Status == CONSOLE_STATUS_WAIT)
        {
            FAIL_FAST_IF_NULL(waiter.get());
            Status = STATUS_SUCCESS;
        }

        RETURN_NTSTATUS(Status);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes non-Unicode formatted data into the given console output object.
// - This method will convert from the given input into wide characters before chain calling the wide character version of the function.
//   It uses the current Output Codepage for conversions (set via SetConsoleOutputCP).
// - NOTE: This may be blocked for various console states and will return a wait context pointer if necessary.
// Arguments:
// - context - the console output object to write the new text into
// - buffer - char/byte text buffer provided by client application to insert
// - read - character count of the number of characters (also bytes because A version) we were able to insert before returning
// - waiter - If we are blocked from writing now and need to wait, this is filled with contextual data for the server to restore the call later
// Return Value:
// - S_OK if successful.
// - S_OK if we need to wait (check if ppWaiter is not nullptr).
// - Or a suitable HRESULT code for math/string/memory failures.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleAImpl(IConsoleOutputObject& context,
                                                     const std::string_view buffer,
                                                     size_t& read,
                                                     std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // Ensure output variables are initialized.
        read = 0;
        waiter.reset();

        bool fLeadByteCaptured = false;
        bool fLeadByteConsumed = false;

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        if (buffer.size() == 0)
        {
            return S_OK;
        }

        const auto codepage = gci.OutputCP;

        // Convert our input parameters to Unicode
        std::unique_ptr<wchar_t[]> wideCharBuffer{ nullptr };
        static Utf8ToWideCharParser parser{ gci.OutputCP };

        // update current codepage in case it was changed from last time
        // this was called. We do this outside the UTF-8 check because the parser drops its state
        // when the codepage changes.
        parser.SetCodePage(gci.OutputCP);

        SCREEN_INFORMATION& ScreenInfo = context.GetActiveBuffer();
        wchar_t* pwchBuffer;
        size_t cchBuffer;
        if (codepage == CP_UTF8)
        {
            wideCharBuffer.release();
            unsigned int charCount;
            unsigned int charsConsumed;
            unsigned int charsGenerated;
            RETURN_IF_FAILED(SizeTToUInt(buffer.size(), &charCount));
            RETURN_IF_FAILED(parser.Parse(reinterpret_cast<const byte*>(buffer.data()),
                                          charCount,
                                          charsConsumed,
                                          wideCharBuffer,
                                          charsGenerated));

            pwchBuffer = reinterpret_cast<wchar_t*>(wideCharBuffer.get());
            cchBuffer = charsGenerated;
            read = charsConsumed;
        }
        else
        {
            NTSTATUS Status = STATUS_SUCCESS;
            PWCHAR TransBuffer;
            PWCHAR TransBufferOriginalLocation;
            DWORD Length;
            ULONG dbcsNumBytes = 0;
            ULONG BufPtrNumBytes = 0;
            const char* BufPtr = buffer.data();

            // (cchTextBufferLength + 2) I think because we might be shoving another unicode char
            // from ScreenInfo->WriteConsoleDbcsLeadByte in front
            TransBuffer = new WCHAR[buffer.size() + 2];
            RETURN_IF_NULL_ALLOC(TransBuffer);
            ZeroMemory(TransBuffer, sizeof(WCHAR) * (buffer.size() + 2));

            TransBufferOriginalLocation = TransBuffer;

            unsigned int uiTextBufferLength;
            RETURN_IF_FAILED(SizeTToUInt(buffer.size(), &uiTextBufferLength));

            if (!ScreenInfo.WriteConsoleDbcsLeadByte[0] || *(PUCHAR)BufPtr < (UCHAR)' ')
            {
                dbcsNumBytes = 0;
                BufPtrNumBytes = uiTextBufferLength;
            }
            else if (buffer.size())
            {
                // there was a portion of a dbcs character stored from a previous
                // call so we take the 2nd half from BufPtr[0], put them together
                // and write the wide char to TransBuffer[0]
                ScreenInfo.WriteConsoleDbcsLeadByte[1] = *(PCHAR)BufPtr;

                try
                {
                    const std::string_view leadByte(reinterpret_cast<const char* const>(ScreenInfo.WriteConsoleDbcsLeadByte),
                                                    ARRAYSIZE(ScreenInfo.WriteConsoleDbcsLeadByte));

                    const std::wstring converted = ConvertToW(gci.OutputCP, leadByte);

                    FAIL_FAST_IF(converted.size() != 1);
                    dbcsNumBytes = sizeof(wchar_t);
                    TransBuffer[0] = converted.at(0);
                    BufPtr++;
                }
                catch (...)
                {
                    Status = STATUS_UNSUCCESSFUL;
                    dbcsNumBytes = 0;
                }

                // this looks weird to be always incrementing even if the conversion failed, but this is the
                // original behavior so it's left unchanged.
                TransBuffer++;
                BufPtrNumBytes = uiTextBufferLength - 1;

                // Note that we used a stored lead byte from a previous call in order to complete this write
                // Use this to offset the "number of bytes consumed" calculation at the end by -1 to account
                // for using a byte we had internally, not off the stream.
                fLeadByteConsumed = true;
            }
            else
            {
                // nothing in ScreenInfo->WriteConsoleDbcsLeadByte and nothing in BufPtr
                BufPtrNumBytes = 0;
            }

            ScreenInfo.WriteConsoleDbcsLeadByte[0] = 0;

            // if the last byte in BufPtr is a lead byte for the current code page,
            // save it for the next time this function is called and we can piece it
            // back together then
            __analysis_assume(BufPtrNumBytes <= uiTextBufferLength);
            if (BufPtrNumBytes && CheckBisectStringA((PCHAR)BufPtr, BufPtrNumBytes, &gci.OutputCPInfo))
            {
                ScreenInfo.WriteConsoleDbcsLeadByte[0] = *((PCHAR)BufPtr + BufPtrNumBytes - 1);
                BufPtrNumBytes--;

                // Note that we captured a lead byte during this call, but won't actually draw it until later.
                // Use this to offset the "number of bytes consumed" calculation at the end by +1 to account
                // for taking a byte off the stream.
                fLeadByteCaptured = true;
            }

            if (BufPtrNumBytes != 0)
            {
                // convert the remaining bytes in BufPtr to wide chars
                Length = sizeof(WCHAR) * MultiByteToWideChar(gci.OutputCP,
                                                             0,
                                                             (LPCCH)BufPtr,
                                                             BufPtrNumBytes,
                                                             TransBuffer,
                                                             BufPtrNumBytes);

                if (Length == 0)
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                BufPtrNumBytes = Length;
            }

            pwchBuffer = TransBufferOriginalLocation;
            cchBuffer = (dbcsNumBytes + BufPtrNumBytes) / sizeof(wchar_t);
        }

        // Make the W version of the call
        size_t cchBufferRead;

        // Hold the specific version of the waiter locally so we can tinker with it if we must to store additional context.
        std::unique_ptr<WriteData> writeDataWaiter;

        HRESULT const hr = WriteConsoleWImplHelper(ScreenInfo, { pwchBuffer, cchBuffer }, cchBufferRead, writeDataWaiter);

        // If there is no waiter, process the byte count now.
        if (nullptr == writeDataWaiter.get())
        {
            // Calculate how many bytes of the original A buffer were consumed in the W version of the call to satisfy pcchTextBufferRead.
            // For UTF-8 conversions, we've already returned this information above.
            if (CP_UTF8 != codepage)
            {
                size_t cchTextBufferRead = 0;

                // Start by counting the number of A bytes we used in printing our W string to the screen.
                try
                {
                    cchTextBufferRead = GetALengthFromW(codepage, { pwchBuffer, cchBufferRead });
                }
                CATCH_LOG();

                // If we captured a byte off the string this time around up above, it means we didn't feed
                // it into the WriteConsoleW above, and therefore its consumption isn't accounted for
                // in the count we just made. Add +1 to compensate.
                if (fLeadByteCaptured)
                {
                    cchTextBufferRead++;
                }

                // If we consumed an internally-stored lead byte this time around up above, it means that we
                // fed a byte into WriteConsoleW that wasn't a part of this particular call's request.
                // We need to -1 to compensate and tell the caller the right number of bytes consumed this request.
                if (fLeadByteConsumed)
                {
                    cchTextBufferRead--;
                }

                read = cchTextBufferRead;
            }
        }
        else
        {
            // If there is a waiter, then we need to stow some additional information in the wait structure so
            // we can synthesize the correct byte count later when the wait routine is triggered.
            if (CP_UTF8 != codepage)
            {
                // For non-UTF8 codepages, save the lead byte captured/consumed data so we can +1 or -1 the final decoded count
                // in the WaitData::Notify method later.
                writeDataWaiter->SetLeadByteAdjustmentStatus(fLeadByteCaptured, fLeadByteConsumed);
            }
            else
            {
                // For UTF8 codepages, just remember the consumption count from the UTF-8 parser.
                writeDataWaiter->SetUtf8ConsumedCharacters(read);
            }
        }

        // Free remaining data
        if (codepage != CP_UTF8)
        {
            delete[] pwchBuffer;
        }

        // Give back the waiter now that we're done with tinkering with it.
        waiter.reset(writeDataWaiter.release());

        return hr;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes Unicode formatted data into the given console output object.
// - NOTE: This may be blocked for various console states and will return a wait context pointer if necessary.
// Arguments:
// - OutContext - the console output object to write the new text into
// - pwsTextBuffer - wide character text buffer provided by client application to insert
// - cchTextBufferLength - text buffer counted in characters
// - pcchTextBufferRead - character count of the number of characters we were able to insert before returning
// - ppWaiter - If we are blocked from writing now and need to wait, this is filled with contextual data for the server to restore the call later
// Return Value:
// - S_OK if successful.
// - S_OK if we need to wait (check if ppWaiter is not nullptr).
// - Or a suitable HRESULT code for math/string/memory failures.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleWImpl(IConsoleOutputObject& context,
                                                     const std::wstring_view buffer,
                                                     size_t& read,
                                                     std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        std::unique_ptr<WriteData> writeDataWaiter;
        RETURN_IF_FAILED(WriteConsoleWImplHelper(context.GetActiveBuffer(), buffer, read, writeDataWaiter));

        // Transfer specific waiter pointer into the generic interface wrapper.
        waiter.reset(writeDataWaiter.release());

        return S_OK;
    }
    CATCH_RETURN();
}
