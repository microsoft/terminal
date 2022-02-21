// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "outputStream.hpp"

#include "_stream.h"
#include "getset.h"
#include "directio.h"
#include "output.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::StateMachine;
using Microsoft::Console::VirtualTerminal::TerminalInput;

ConhostInternalGetSet::ConhostInternalGetSet(_In_ IIoProvider& io) :
    _io{ io }
{
}

// Routine Description:
// - Handles the print action from the state machine
// Arguments:
// - string - The string to be printed.
// Return Value:
// - <none>
void ConhostInternalGetSet::PrintString(const std::wstring_view string)
{
    size_t dwNumBytes = string.size() * sizeof(wchar_t);

    Cursor& cursor = _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    if (!cursor.IsOn())
    {
        cursor.SetIsOn(true);
    }

    // Defer the cursor drawing while we are iterating the string, for a better performance.
    // We can not waste time displaying a cursor event when we know more text is coming right behind it.
    cursor.StartDeferDrawing();
    const auto ntstatus = WriteCharsLegacy(_io.GetActiveOutputBuffer(),
                                           string.data(),
                                           string.data(),
                                           string.data(),
                                           &dwNumBytes,
                                           nullptr,
                                           _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition().X,
                                           WC_LIMIT_BACKSPACE | WC_DELAY_EOL_WRAP,
                                           nullptr);
    cursor.EndDeferDrawing();

    THROW_IF_NTSTATUS_FAILED(ntstatus);
}

// Routine Description:
// - Connects the GetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - screenBufferInfo - Structure to hold screen buffer information like the public API call.
// Return Value:
// - <none>
void ConhostInternalGetSet::GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const
{
    ServiceLocator::LocateGlobals().api.GetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), screenBufferInfo);
}

// Routine Description:
// - Connects the SetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - screenBufferInfo - Structure containing screen buffer information like the public API call.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo)
{
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api.SetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), screenBufferInfo));
}

// Routine Description:
// - Connects the SetCursorPosition API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - position - new cursor position to set like the public API call.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCursorPosition(const COORD position)
{
    auto& info = _io.GetActiveOutputBuffer();
    const auto clampedPosition = info.GetTextBuffer().ClampPositionWithinLine(position);
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api.SetConsoleCursorPositionImpl(info, clampedPosition));
}

// Method Description:
// - Retrieves the current TextAttribute of the active screen buffer.
// Arguments:
// - <none>
// Return Value:
// - the TextAttribute value.
TextAttribute ConhostInternalGetSet::GetTextAttributes() const
{
    return _io.GetActiveOutputBuffer().GetAttributes();
}

// Method Description:
// - Sets the current TextAttribute of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - attrs: The new TextAttribute to use
// Return Value:
// - <none>
void ConhostInternalGetSet::SetTextAttributes(const TextAttribute& attrs)
{
    _io.GetActiveOutputBuffer().SetAttributes(attrs);
}

// Method Description:
// - Sets the line rendition attribute for the current row of the active screen
//   buffer. This controls how character cells are scaled when the row is rendered.
// Arguments:
// - lineRendition: The new LineRendition attribute to use
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCurrentLineRendition(const LineRendition lineRendition)
{
    auto& textBuffer = _io.GetActiveOutputBuffer().GetTextBuffer();
    textBuffer.SetCurrentLineRendition(lineRendition);
}

// Method Description:
// - Resets the line rendition attribute to SingleWidth for a specified range
//   of row numbers.
// Arguments:
// - startRow: The row number of first line to be modified
// - endRow: The row number following the last line to be modified
// Return Value:
// - <none>
void ConhostInternalGetSet::ResetLineRenditionRange(const size_t startRow, const size_t endRow)
{
    auto& textBuffer = _io.GetActiveOutputBuffer().GetTextBuffer();
    textBuffer.ResetLineRenditionRange(startRow, endRow);
}

// Method Description:
// - Returns the number of cells that will fit on the specified row when
//   rendered with its current line rendition.
// Arguments:
// - row: The row number of the line to measure
// Return Value:
// - the number of cells that will fit on the line
SHORT ConhostInternalGetSet::GetLineWidth(const size_t row) const
{
    const auto& textBuffer = _io.GetActiveOutputBuffer().GetTextBuffer();
    return textBuffer.GetLineWidth(row);
}

// Routine Description:
// - Writes events to the input buffer already formed into IInputEvents
// Arguments:
// - events - the input events to be copied into the head of the input
//            buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - <none>
void ConhostInternalGetSet::WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten)
{
    eventsWritten = _io.GetActiveInputBuffer()->Write(events);
}

// Routine Description:
// - Connects the SetWindowInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - absolute - Should the window be moved to an absolute position? If false, the movement is relative to the current pos.
// - window - Info about how to move the viewport
// Return Value:
// - <none>
void ConhostInternalGetSet::SetWindowInfo(const bool absolute, const SMALL_RECT& window)
{
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api.SetConsoleWindowInfoImpl(_io.GetActiveOutputBuffer(), absolute, window));
}

// Routine Description:
// - Sets the various terminal input modes.
//   SetInputMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - mode - the input mode to change.
// - enabled - set to true to enable the mode, false to disable it.
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::SetInputMode(const TerminalInput::Mode mode, const bool enabled)
{
    auto& terminalInput = _io.GetActiveInputBuffer()->GetTerminalInput();
    terminalInput.SetInputMode(mode, enabled);

    // If we're a conpty, AND WE'RE IN VT INPUT MODE, always pass input mode requests
    // The VT Input mode check is to work around ssh.exe v7.7, which uses VT
    // output, but not Input.
    // The original comment said, "Once the conpty supports these types of input,
    // this check can be removed. See GH#4911". Unfortunately, time has shown
    // us that SSH 7.7 _also_ requests mouse input and that can have a user interface
    // impact on the actual connected terminal. We can't remove this check,
    // because SSH <=7.7 is out in the wild on all versions of Windows <=2004.
    return !(IsConsolePty() && IsVtInputEnabled());
}

// Routine Description:
// - Sets the various StateMachine parser modes.
//   SetParserMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - mode - the parser mode to change.
// - enabled - set to true to enable the mode, false to disable it.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetParserMode(const StateMachine::Mode mode, const bool enabled)
{
    auto& stateMachine = _io.GetActiveOutputBuffer().GetStateMachine();
    stateMachine.SetParserMode(mode, enabled);
}

// Routine Description:
// - Retrieves the various StateMachine parser modes.
//   GetParserMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - mode - the parser mode to query.
// Return Value:
// - true if the mode is enabled. false if disabled.
bool ConhostInternalGetSet::GetParserMode(const Microsoft::Console::VirtualTerminal::StateMachine::Mode mode) const
{
    auto& stateMachine = _io.GetActiveOutputBuffer().GetStateMachine();
    return stateMachine.GetParserMode(mode);
}

// Routine Description:
// - Sets the various render modes.
// Arguments:
// - mode - the render mode to change.
// - enabled - set to true to enable the mode, false to disable it.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetRenderMode(const RenderSettings::Mode mode, const bool enabled)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderSettings = g.getConsoleInformation().GetRenderSettings();
    renderSettings.SetRenderMode(mode, enabled);

    if (g.pRender)
    {
        g.pRender->TriggerRedrawAll();
    }
}

// Routine Description:
// - Sets the ENABLE_WRAP_AT_EOL_OUTPUT mode. This controls whether the cursor moves
//     to the beginning of the next row when it reaches the end of the current row.
// Arguments:
// - wrapAtEOL - set to true to wrap, false to overwrite the last character.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetAutoWrapMode(const bool wrapAtEOL)
{
    auto& outputMode = _io.GetActiveOutputBuffer().OutputMode;
    WI_UpdateFlag(outputMode, ENABLE_WRAP_AT_EOL_OUTPUT, wrapAtEOL);
}

// Routine Description:
// - Makes the cursor visible or hidden. Does not modify blinking state.
// Arguments:
// - visible - set to true to make the cursor visible, false to hide.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCursorVisibility(const bool visible)
{
    auto& textBuffer = _io.GetActiveOutputBuffer().GetTextBuffer();
    textBuffer.GetCursor().SetIsVisible(visible);
}

// Routine Description:
// - Enables or disables the cursor blinking.
// Arguments:
// - fEnable - set to true to enable blinking, false to disable
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::EnableCursorBlinking(const bool fEnable)
{
    auto& textBuffer = _io.GetActiveOutputBuffer().GetTextBuffer();
    textBuffer.GetCursor().SetBlinkingAllowed(fEnable);

    // GH#2642 - From what we've gathered from other terminals, when blinking is
    // disabled, the cursor should remain On always, and have the visibility
    // controlled by the IsVisible property. So when you do a printf "\e[?12l"
    // to disable blinking, the cursor stays stuck On. At this point, only the
    // cursor visibility property controls whether the user can see it or not.
    // (Yes, the cursor can be On and NOT Visible)
    textBuffer.GetCursor().SetIsOn(true);

    // If we are connected to a pty, return that we could not handle this
    // so that the VT sequence gets flushed to terminal.
    return !IsConsolePty();
}

// Routine Description:
// - Sets the top and bottom scrolling margins for the current page. This creates
//     a subsection of the screen that scrolls when input reaches the end of the
//     region, leaving the rest of the screen untouched.
// Arguments:
// - scrollMargins - A rect who's Top and Bottom members will be used to set
//     the new values of the top and bottom margins. If (0,0), then the margins
//     will be disabled. NOTE: This is a rect in the case that we'll need the
//     left and right margins in the future.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetScrollingRegion(const SMALL_RECT& scrollMargins)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto srScrollMargins = screenInfo.GetRelativeScrollMargins().ToInclusive();
    srScrollMargins.Top = scrollMargins.Top;
    srScrollMargins.Bottom = scrollMargins.Bottom;
    screenInfo.SetScrollMargins(Viewport::FromInclusive(srScrollMargins));
}

// Method Description:
// - Retrieves the current Line Feed/New Line (LNM) mode.
// Arguments:
// - None
// Return Value:
// - true if a line feed also produces a carriage return. false otherwise.
bool ConhostInternalGetSet::GetLineFeedMode() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.IsReturnOnNewlineAutomatic();
}

// Routine Description:
// - Performs a line feed, possibly preceded by carriage return.
// Arguments:
// - withReturn - Set to true if a carriage return should be performed as well.
// Return Value:
// - <none>
void ConhostInternalGetSet::LineFeed(const bool withReturn)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto& textBuffer = screenInfo.GetTextBuffer();
    auto cursorPosition = textBuffer.GetCursor().GetPosition();

    // We turn the cursor on before an operation that might scroll the viewport, otherwise
    // that can result in an old copy of the cursor being left behind on the screen.
    textBuffer.GetCursor().SetIsOn(true);

    // Since we are explicitly moving down a row, clear the wrap status on the row we're leaving
    textBuffer.GetRowByOffset(cursorPosition.Y).SetWrapForced(false);

    cursorPosition.Y += 1;
    if (withReturn)
    {
        cursorPosition.X = 0;
    }
    else
    {
        cursorPosition = textBuffer.ClampPositionWithinLine(cursorPosition);
    }

    THROW_IF_NTSTATUS_FAILED(AdjustCursorPosition(screenInfo, cursorPosition, FALSE, nullptr));
}

// Routine Description:
// - Sends a notify message to play the "SystemHand" sound event.
// Return Value:
// - <none>
void ConhostInternalGetSet::WarningBell()
{
    _io.GetActiveOutputBuffer().SendNotifyBeep();
}

// Routine Description:
// - Performs a "Reverse line feed", essentially, the opposite of '\n'.
// Return Value:
// - <none>
void ConhostInternalGetSet::ReverseLineFeed()
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    const SMALL_RECT viewport = screenInfo.GetViewport().ToInclusive();
    const COORD oldCursorPosition = screenInfo.GetTextBuffer().GetCursor().GetPosition();
    COORD newCursorPosition = { oldCursorPosition.X, oldCursorPosition.Y - 1 };
    newCursorPosition = screenInfo.GetTextBuffer().ClampPositionWithinLine(newCursorPosition);

    // If the cursor is at the top of the viewport, we don't want to shift the viewport up.
    // We want it to stay exactly where it is.
    // In that case, shift the buffer contents down, to emulate inserting a line
    //      at the top of the buffer.
    if (oldCursorPosition.Y > viewport.Top)
    {
        // Cursor is below the top line of the viewport
        THROW_IF_NTSTATUS_FAILED(AdjustCursorPosition(screenInfo, newCursorPosition, TRUE, nullptr));
    }
    else
    {
        // If we don't have margins, or the cursor is within the boundaries of the margins
        // It's important to check if the cursor is in the margins,
        //      If it's not, but the margins are set, then we don't want to scroll anything
        if (screenInfo.IsCursorInMargins(oldCursorPosition))
        {
            // Cursor is at the top of the viewport
            // Rectangle to cut out of the existing buffer. This is inclusive.
            // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
            SMALL_RECT srScroll;
            srScroll.Left = 0;
            srScroll.Right = SHORT_MAX;
            srScroll.Top = viewport.Top;
            srScroll.Bottom = viewport.Bottom;
            // Clip to the DECSTBM margin boundary
            if (screenInfo.AreMarginsSet())
            {
                srScroll.Bottom = screenInfo.GetAbsoluteScrollMargins().BottomInclusive();
            }
            // Paste coordinate for cut text above
            COORD coordDestination;
            coordDestination.X = 0;
            coordDestination.Y = viewport.Top + 1;

            // Note the revealed lines are filled with the standard erase attributes.
            ScrollRegion(srScroll, srScroll, coordDestination, true);
        }
    }
}

// Routine Description:
// - Sets the title of the console window.
// Arguments:
// - title - The null-terminated string to set as the window title
// Return Value:
// - <none>
void ConhostInternalGetSet::SetWindowTitle(std::wstring_view title)
{
    ServiceLocator::LocateGlobals().getConsoleInformation().SetTitle(title);
}

// Routine Description:
// - Swaps to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. This creates a new alternate, and switches to it.
//     If there is an already existing alternate, it is discarded.
// Return Value:
// - <none>
void ConhostInternalGetSet::UseAlternateScreenBuffer()
{
    THROW_IF_NTSTATUS_FAILED(_io.GetActiveOutputBuffer().UseAlternateScreenBuffer());
}

// Routine Description:
// - Swaps to the main screen buffer. From the alternate buffer, returns to the main screen
//     buffer. From the main screen buffer, does nothing. The alternate is discarded.
// Return Value:
// - <none>
void ConhostInternalGetSet::UseMainScreenBuffer()
{
    _io.GetActiveOutputBuffer().UseMainScreenBuffer();
}

// Routine Description:
// - Performs a VT-style erase all operation on the buffer.
//      See SCREEN_INFORMATION::VtEraseAll's description for details.
// Return Value:
// - <none>
void ConhostInternalGetSet::EraseAll()
{
    THROW_IF_FAILED(_io.GetActiveOutputBuffer().VtEraseAll());
}

// Routine Description:
// - Clears the entire contents of the viewport, except for the cursor's row,
//     which is moved to the top line of the viewport.
//     See SCREEN_INFORMATION::ClearBuffer's description for details.
// Return Value:
// - <none>
void ConhostInternalGetSet::ClearBuffer()
{
    THROW_IF_FAILED(_io.GetActiveOutputBuffer().ClearBuffer());
}

// Method Description:
// - Retrieves the current user default cursor style.
// Arguments:
// - <none>
// Return Value:
// - the default cursor style.
CursorType ConhostInternalGetSet::GetUserDefaultCursorStyle() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetCursorType();
}

// Routine Description:
// - Sets the current cursor style.
// Arguments:
// - style: The style of cursor to change the cursor to.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCursorStyle(const CursorType style)
{
    _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().SetType(style);
}

// Routine Description:
// - Forces the renderer to repaint the screen. If the input screen buffer is
//      not the active one, then just do nothing. We only want to redraw the
//      screen buffer that requested the repaint, and switching screen buffers
//      will already force a repaint.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConhostInternalGetSet::RefreshWindow()
{
    auto& g = ServiceLocator::LocateGlobals();
    if (&_io.GetActiveOutputBuffer() == &g.getConsoleInformation().GetActiveOutputBuffer())
    {
        g.pRender->TriggerRedrawAll();
    }
}

// Routine Description:
// - Writes the input KeyEvent to the console as a console control event. This
//      can be used for potentially generating Ctrl-C events, as
//      HandleGenericKeyEvent will correctly generate the Ctrl-C response in
//      the same way that it'd be handled from the window proc, with the proper
//      processed vs raw input handling.
//  If the input key is *not* a Ctrl-C key, then it will get written to the
//      buffer just the same as any other KeyEvent.
// Arguments:
// - key - The keyevent to send to the console.
// Return Value:
// - <none>
void ConhostInternalGetSet::WriteControlInput(const KeyEvent key)
{
    HandleGenericKeyEvent(key, false);
}

// Routine Description:
// - Connects the SetConsoleOutputCP API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - codepage - the new output codepage of the console.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetConsoleOutputCP(const unsigned int codepage)
{
    THROW_IF_FAILED(DoSrvSetConsoleOutputCodePage(codepage));
}

// Routine Description:
// - Gets the codepage used for translating text when calling A versions of functions affecting the output buffer.
// Arguments:
// - <none>
// Return Value:
// - the outputCP of the console.
unsigned int ConhostInternalGetSet::GetConsoleOutputCP() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
}

// Routine Description:
// - Resizes the window to the specified dimensions, in characters.
// Arguments:
// - width: The new width of the window, in columns
// - height: The new height of the window, in rows
// Return Value:
// - True if handled successfully. False otherwise.
bool ConhostInternalGetSet::ResizeWindow(const size_t width, const size_t height)
{
    SHORT sColumns = 0;
    SHORT sRows = 0;

    THROW_IF_FAILED(SizeTToShort(width, &sColumns));
    THROW_IF_FAILED(SizeTToShort(height, &sRows));
    // We should do nothing if 0 is passed in for a size.
    RETURN_BOOL_IF_FALSE(width > 0 && height > 0);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    GetConsoleScreenBufferInfoEx(csbiex);

    const Viewport oldViewport = Viewport::FromInclusive(csbiex.srWindow);
    const Viewport newViewport = Viewport::FromDimensions(oldViewport.Origin(), sColumns, sRows);
    // Always resize the width of the console
    csbiex.dwSize.X = sColumns;
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = sRows;
    }

    // SetWindowInfo expect inclusive rects
    const auto sri = newViewport.ToInclusive();

    // SetConsoleScreenBufferInfoEx however expects exclusive rects
    const auto sre = newViewport.ToExclusive();
    csbiex.srWindow = sre;

    SetConsoleScreenBufferInfoEx(csbiex);
    SetWindowInfo(true, sri);
    return true;
}

// Routine Description:
// - Forces the VT Renderer to NOT paint the next resize event. This is used by
//      InteractDispatch, to prevent resizes from echoing between terminal and host.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConhostInternalGetSet::SuppressResizeRepaint()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    THROW_IF_FAILED(gci.GetVtIo()->SuppressResizeRepaint());
}

// Routine Description:
// - Checks if the console host is acting as a pty.
// Arguments:
// - <none>
// Return Value:
// - true if we're in pty mode.
bool ConhostInternalGetSet::IsConsolePty() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().IsInVtIoMode();
}

// Routine Description:
// - Deletes lines in the active screen buffer.
// Parameters:
// - count - the number of lines to delete
void ConhostInternalGetSet::DeleteLines(const size_t count)
{
    _modifyLines(count, false);
}

// Routine Description:
// - Inserts lines in the active screen buffer.
// Parameters:
// - count - the number of lines to insert
void ConhostInternalGetSet::InsertLines(const size_t count)
{
    _modifyLines(count, true);
}

// Routine Description:
// - internal logic for adding or removing lines in the active screen buffer
//   this also moves the cursor to the left margin, which is expected behavior for IL and DL
// Parameters:
// - count - the number of lines to modify
// - insert - true if inserting lines, false if deleting lines
void ConhostInternalGetSet::_modifyLines(const size_t count, const bool insert)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto& textBuffer = screenInfo.GetTextBuffer();
    const auto cursorPosition = textBuffer.GetCursor().GetPosition();
    if (screenInfo.IsCursorInMargins(cursorPosition))
    {
        // Rectangle to cut out of the existing buffer. This is inclusive.
        // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
        SMALL_RECT srScroll;
        srScroll.Left = 0;
        srScroll.Right = SHORT_MAX;
        srScroll.Top = cursorPosition.Y;
        srScroll.Bottom = screenInfo.GetViewport().BottomInclusive();
        // Clip to the DECSTBM margin boundary
        if (screenInfo.AreMarginsSet())
        {
            srScroll.Bottom = screenInfo.GetAbsoluteScrollMargins().BottomInclusive();
        }
        // Paste coordinate for cut text above
        COORD coordDestination;
        coordDestination.X = 0;
        if (insert)
        {
            coordDestination.Y = (cursorPosition.Y) + gsl::narrow<short>(count);
        }
        else
        {
            coordDestination.Y = (cursorPosition.Y) - gsl::narrow<short>(count);
        }

        // Note the revealed lines are filled with the standard erase attributes.
        ScrollRegion(srScroll, srScroll, coordDestination, true);

        // The IL and DL controls are also expected to move the cursor to the left margin.
        // For now this is just column 0, since we don't yet support DECSLRM.
        THROW_IF_NTSTATUS_FAILED(screenInfo.SetCursorPosition({ 0, cursorPosition.Y }, false));
    }
}

// Method Description:
// - Snaps the screen buffer's viewport to the "virtual bottom", the last place
//      the viewport was before the user scrolled it (with the mouse or scrollbar)
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConhostInternalGetSet::MoveToBottom()
{
    _io.GetActiveOutputBuffer().MoveToBottom();
}

// Method Description:
// - Retrieves the value in the colortable at the specified index.
// Arguments:
// - tableIndex: the index of the color table to retrieve.
// Return Value:
// - the COLORREF value for the color at that index in the table.
COLORREF ConhostInternalGetSet::GetColorTableEntry(const size_t tableIndex) const
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    return gci.GetColorTableEntry(tableIndex);
}

// Method Description:
// - Updates the value in the colortable at index tableIndex to the new color
//   color. color is a COLORREF, format 0x00BBGGRR.
// Arguments:
// - tableIndex: the index of the color table to update.
// - color: the new COLORREF to use as that color table value.
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::SetColorTableEntry(const size_t tableIndex, const COLORREF color)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    gci.SetColorTableEntry(tableIndex, color);

    // Update the screen colors if we're not a pty
    // No need to force a redraw in pty mode.
    if (g.pRender && !gci.IsInVtIoMode())
    {
        g.pRender->TriggerRedrawAll();
    }

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    return !gci.IsInVtIoMode();
}

// Routine Description:
// - Sets the position in the color table for the given color alias.
// Arguments:
// - alias: the color alias to update.
// - tableIndex: the new position of the alias in the color table.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex)
{
    auto& renderSettings = ServiceLocator::LocateGlobals().getConsoleInformation().GetRenderSettings();
    renderSettings.SetColorAliasIndex(alias, tableIndex);
}

// Routine Description:
// - Fills a region of the screen buffer.
// Arguments:
// - startPosition - The position to begin filling at.
// - fillLength - The number of characters to fill.
// - fillChar - Character to fill the target region with.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - <none>
void ConhostInternalGetSet::FillRegion(const COORD startPosition,
                                       const size_t fillLength,
                                       const wchar_t fillChar,
                                       const bool standardFillAttrs)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();

    if (fillLength == 0)
    {
        return;
    }

    // For most VT erasing operations, the standard requires that the
    // erased area be filled with the current background color, but with
    // no additional meta attributes set. For all other cases, we just
    // fill with the default attributes.
    auto fillAttrs = TextAttribute{};
    if (standardFillAttrs)
    {
        fillAttrs = screenInfo.GetAttributes();
        fillAttrs.SetStandardErase();
    }

    const auto fillData = OutputCellIterator{ fillChar, fillAttrs, fillLength };
    screenInfo.Write(fillData, startPosition, false);

    // Notify accessibility
    if (screenInfo.HasAccessibilityEventing())
    {
        auto endPosition = startPosition;
        const auto bufferSize = screenInfo.GetBufferSize();
        bufferSize.MoveInBounds(fillLength - 1, endPosition);
        screenInfo.NotifyAccessibilityEventing(startPosition.X, startPosition.Y, endPosition.X, endPosition.Y);
    }
}

// Routine Description:
// - Moves a block of data in the screen buffer, optionally limiting the effects
//      of the move to a clipping rectangle.
// Arguments:
// - scrollRect - Region to copy/move (source and size).
// - clipRect - Optional clip region to contain buffer change effects.
// - destinationOrigin - Upper left corner of target region.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - <none>
void ConhostInternalGetSet::ScrollRegion(const SMALL_RECT scrollRect,
                                         const std::optional<SMALL_RECT> clipRect,
                                         const COORD destinationOrigin,
                                         const bool standardFillAttrs)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();

    // For most VT scrolling operations, the standard requires that the
    // erased area be filled with the current background color, but with
    // no additional meta attributes set. For all other cases, we just
    // fill with the default attributes.
    auto fillAttrs = TextAttribute{};
    if (standardFillAttrs)
    {
        fillAttrs = screenInfo.GetAttributes();
        fillAttrs.SetStandardErase();
    }

    ::ScrollRegion(screenInfo, scrollRect, clipRect, destinationOrigin, UNICODE_SPACE, fillAttrs);
}

// Routine Description:
// - Checks if the InputBuffer is willing to accept VT Input directly
//   IsVtInputEnabled is an internal-only "API" call that the vt commands can execute,
//    but it is not represented as a function call on our public API surface.
// Arguments:
// - <none>
// Return value:
// - true if enabled (see IsInVirtualTerminalInputMode). false otherwise.
bool ConhostInternalGetSet::IsVtInputEnabled() const
{
    return _io.GetActiveInputBuffer()->IsInVirtualTerminalInputMode();
}

// Method Description:
// - Updates the buffer's current text attributes depending on whether we are
//   starting/ending a hyperlink
// Arguments:
// - The hyperlink URI
// Return Value:
// - <none>
void ConhostInternalGetSet::AddHyperlink(const std::wstring_view uri, const std::wstring_view params) const
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto attr = screenInfo.GetAttributes();
    const auto id = screenInfo.GetTextBuffer().GetHyperlinkId(uri, params);
    attr.SetHyperlinkId(id);
    screenInfo.GetTextBuffer().SetCurrentAttributes(attr);
    screenInfo.GetTextBuffer().AddHyperlinkToMap(uri, id);
}

void ConhostInternalGetSet::EndHyperlink() const
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto attr = screenInfo.GetAttributes();
    attr.SetHyperlinkId(0);
    screenInfo.GetTextBuffer().SetCurrentAttributes(attr);
}

// Routine Description:
// - Replaces the active soft font with the given bit pattern.
// Arguments:
// - bitPattern - An array of scanlines representing all the glyphs in the font.
// - cellSize - The cell size for an individual glyph.
// - centeringHint - The horizontal extent that glyphs are offset from center.
// Return Value:
// - <none>
void ConhostInternalGetSet::UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                                           const SIZE cellSize,
                                           const size_t centeringHint)
{
    auto* pRender = ServiceLocator::LocateGlobals().pRender;
    if (pRender)
    {
        pRender->UpdateSoftFont(bitPattern, cellSize, centeringHint);
    }
}
