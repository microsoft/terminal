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
using namespace Microsoft::Console::VirtualTerminal;
using Microsoft::Console::Interactivity::ServiceLocator;

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
    auto dwNumBytes = string.size() * sizeof(wchar_t);

    auto& cursor = _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
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

// - Sends a string response to the input stream of the console.
// - Used by various commands where the program attached would like a reply to one of the commands issued.
// - This will generate two "key presses" (one down, one up) for every character in the string and place them into the head of the console's input stream.
// Arguments:
// - response - The response string to transmit back to the input stream
// Return Value:
// - <none>
void ConhostInternalGetSet::ReturnResponse(const std::wstring_view response)
{
    std::deque<std::unique_ptr<IInputEvent>> inEvents;

    // generate a paired key down and key up event for every
    // character to be sent into the console's input buffer
    for (const auto& wch : response)
    {
        // This wasn't from a real keyboard, so we're leaving key/scan codes blank.
        KeyEvent keyEvent{ TRUE, 1, 0, 0, wch, 0 };

        inEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
        keyEvent.SetKeyDown(false);
        inEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
    }

    // TODO GH#4954 During the input refactor we may want to add a "priority" input list
    // to make sure that "response" input is spooled directly into the application.
    // We switched this to an append (vs. a prepend) to fix GH#1637, a bug where two CPR
    // could collide with each other.
    _io.GetActiveInputBuffer()->Write(inEvents);
}

// Routine Description:
// - Retrieves the state machine for the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - a reference to the StateMachine instance.
StateMachine& ConhostInternalGetSet::GetStateMachine()
{
    return _io.GetActiveOutputBuffer().GetStateMachine();
}

// Routine Description:
// - Retrieves the text buffer for the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - a reference to the TextBuffer instance.
TextBuffer& ConhostInternalGetSet::GetTextBuffer()
{
    return _io.GetActiveOutputBuffer().GetTextBuffer();
}

// Routine Description:
// - Retrieves the virtual viewport of the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - the exclusive coordinates of the viewport.
til::rect ConhostInternalGetSet::GetViewport() const
{
    return til::rect{ _io.GetActiveOutputBuffer().GetVirtualViewport().ToInclusive() };
}

// Routine Description:
// - Sets the position of the window viewport.
// Arguments:
// - position - the new position of the viewport.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetViewportPosition(const til::point position)
{
    auto& info = _io.GetActiveOutputBuffer();
    const auto dimensions = til::size{ info.GetViewport().Dimensions() };
    const auto windowRect = til::rect{ position, dimensions }.to_small_rect();
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api->SetConsoleWindowInfoImpl(info, true, windowRect));
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
void ConhostInternalGetSet::SetScrollingRegion(const til::inclusive_rect& scrollMargins)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto srScrollMargins = screenInfo.GetRelativeScrollMargins().ToInclusive();
    srScrollMargins.Top = gsl::narrow<short>(scrollMargins.Top);
    srScrollMargins.Bottom = gsl::narrow<short>(scrollMargins.Bottom);
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
// - Shows or hides the active window when asked.
// Arguments:
// - showOrHide - True for show, False for hide. Matching WM_SHOWWINDOW lParam.
// Return Value:
// - <none>
void ConhostInternalGetSet::ShowWindow(bool showOrHide)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto hwnd = gci.IsInVtIoMode() ? ServiceLocator::LocatePseudoWindow() : ServiceLocator::LocateConsoleWindow()->GetWindowHandle();
    ::ShowWindow(hwnd, showOrHide ? SW_NORMAL : SW_MINIMIZE);
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
// - Sets the XTerm bracketed paste mode. This controls whether pasted content is
//     bracketed with control sequences to differentiate it from typed text.
// Arguments:
// - enable - set to true to enable bracketing, false to disable.
// Return Value:
// - <none>
void ConhostInternalGetSet::EnableXtermBracketedPasteMode(const bool /*enabled*/)
{
    // TODO
}

// Routine Description:
// - Copies the given content to the clipboard.
// Arguments:
// - content - the text to be copied.
// Return Value:
// - <none>
void ConhostInternalGetSet::CopyToClipboard(const std::wstring_view /*content*/)
{
    // TODO
}

// Routine Description:
// - Updates the taskbar progress indicator.
// Arguments:
// - state: indicates the progress state
// - progress: indicates the progress value
// Return Value:
// - <none>
void ConhostInternalGetSet::SetTaskbarProgress(const DispatchTypes::TaskbarState /*state*/, const size_t /*progress*/)
{
    // TODO
}

// Routine Description:
// - Set the active working directory. Not used in conhost.
// Arguments:
// - content - the text to be copied.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetWorkingDirectory(const std::wstring_view /*uri*/)
{
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

    auto api = ServiceLocator::LocateGlobals().api;
    auto& screenInfo = _io.GetActiveOutputBuffer();

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    api->GetConsoleScreenBufferInfoExImpl(screenInfo, csbiex);

    const auto oldViewport = screenInfo.GetVirtualViewport();
    auto newViewport = Viewport::FromDimensions(oldViewport.Origin(), sColumns, sRows);
    // Always resize the width of the console
    csbiex.dwSize.X = sColumns;
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = sRows;
    }

    // If the cursor row is now past the bottom of the viewport, we'll have to
    // move the viewport down to bring it back into view. However, we don't want
    // to do this in pty mode, because the conpty resize operation is dependent
    // on the viewport *not* being adjusted.
    const short cursorOverflow = csbiex.dwCursorPosition.Y - newViewport.BottomInclusive();
    if (cursorOverflow > 0 && !IsConsolePty())
    {
        newViewport = Viewport::Offset(newViewport, { 0, cursorOverflow });
    }

    // SetWindowInfo expect inclusive rects
    const auto sri = newViewport.ToInclusive();

    // SetConsoleScreenBufferInfoEx however expects exclusive rects
    const auto sre = newViewport.ToExclusive();
    csbiex.srWindow = sre;

    THROW_IF_FAILED(api->SetConsoleScreenBufferInfoExImpl(screenInfo, csbiex));
    THROW_IF_FAILED(api->SetConsoleWindowInfoImpl(screenInfo, true, sri));
    return true;
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

// Routine Description:
// - Lets accessibility apps know when an area of the screen has changed.
// Arguments:
// - changedRect - the area that has changed.
// Return value:
// - <none>
void ConhostInternalGetSet::NotifyAccessibilityChange(const til::rect& changedRect)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    if (screenInfo.HasAccessibilityEventing())
    {
        screenInfo.NotifyAccessibilityEventing(
            gsl::narrow_cast<short>(changedRect.left),
            gsl::narrow_cast<short>(changedRect.top),
            gsl::narrow_cast<short>(changedRect.right - 1),
            gsl::narrow_cast<short>(changedRect.bottom - 1));
    }
}
