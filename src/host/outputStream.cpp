// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "outputStream.hpp"

#include "_stream.h"
#include "getset.h"
#include "handle.h"
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
    return _io.GetActiveOutputBuffer().GetVirtualViewport().ToExclusive();
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
    const auto dimensions = info.GetViewport().Dimensions();
    const auto windowRect = til::rect{ position, dimensions }.to_inclusive_rect();
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
// - Retrieves the current state of ENABLE_WRAP_AT_EOL_OUTPUT mode.
// Arguments:
// - <none>
// Return Value:
// - true if the mode is enabled. false otherwise.
bool ConhostInternalGetSet::GetAutoWrapMode() const
{
    const auto outputMode = _io.GetActiveOutputBuffer().OutputMode;
    return WI_IsFlagSet(outputMode, ENABLE_WRAP_AT_EOL_OUTPUT);
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
    srScrollMargins.top = scrollMargins.top;
    srScrollMargins.bottom = scrollMargins.bottom;
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
    auto& screenInfo = _io.GetActiveOutputBuffer();
    return WI_IsFlagClear(screenInfo.OutputMode, DISABLE_NEWLINE_AUTO_RETURN);
}

// Routine Description:
// - Performs a line feed, possibly preceded by carriage return.
// Arguments:
// - withReturn - Set to true if a carriage return should be performed as well.
// - wrapForced - Set to true is the line feed was the result of the line wrapping.
// Return Value:
// - <none>
void ConhostInternalGetSet::LineFeed(const bool withReturn, const bool wrapForced)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto& textBuffer = screenInfo.GetTextBuffer();
    auto cursorPosition = textBuffer.GetCursor().GetPosition();

    // If the line was forced to wrap, set the wrap status.
    // When explicitly moving down a row, clear the wrap status.
    textBuffer.GetRowByOffset(cursorPosition.y).SetWrapForced(wrapForced);

    cursorPosition.y += 1;
    if (withReturn)
    {
        cursorPosition.x = 0;
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

    // GH#13301 - When we send this ShowWindow message, if we send it to the
    // conhost HWND, it's going to need to get processed by the window message
    // thread before returning.
    // However, ShowWindowAsync doesn't have this problem. It'll post the
    // message to the window thread, then immediately return, so we don't have
    // to worry about deadlocking.
    ::ShowWindowAsync(hwnd, showOrHide ? SW_SHOWNOACTIVATE : SW_MINIMIZE);
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
void ConhostInternalGetSet::SetBracketedPasteMode(const bool enabled)
{
    // TODO GH#395: Bracketed Paste Mode is not yet supported in conhost, but we
    // still keep track of the state so it can be reported by DECRQM.
    _bracketedPasteMode = enabled;
}

// Routine Description:
// - Gets the current state of XTerm bracketed paste mode.
// Arguments:
// - <none>
// Return Value:
// - true if the mode is enabled, false if not, nullopt if unsupported.
std::optional<bool> ConhostInternalGetSet::GetBracketedPasteMode() const
{
    // TODO GH#395: Bracketed Paste Mode is not yet supported in conhost, so we
    // only report the state if we're tracking it for conpty.
    return IsConsolePty() ? std::optional{ _bracketedPasteMode } : std::nullopt;
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
// - Plays a single MIDI note, blocking for the duration.
// Arguments:
// - noteNumber - The MIDI note number to be played (0 - 127).
// - velocity - The force with which the note should be played (0 - 127).
// - duration - How long the note should be sustained (in milliseconds).
// Return value:
// - true if successful. false otherwise.
void ConhostInternalGetSet::PlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration)
{
    // Unlock the console, so the UI doesn't hang while we're busy.
    UnlockConsole();

    // This call will block for the duration, unless shutdown early.
    const auto windowHandle = ServiceLocator::LocateConsoleWindow()->GetWindowHandle();
    auto& midiAudio = ServiceLocator::LocateGlobals().getConsoleInformation().GetMidiAudio();
    midiAudio.PlayNote(windowHandle, noteNumber, velocity, std::chrono::duration_cast<std::chrono::milliseconds>(duration));

    LockConsole();
}

// Routine Description:
// - Resizes the window to the specified dimensions, in characters.
// Arguments:
// - width: The new width of the window, in columns
// - height: The new height of the window, in rows
// Return Value:
// - True if handled successfully. False otherwise.
bool ConhostInternalGetSet::ResizeWindow(const til::CoordType sColumns, const til::CoordType sRows)
{
    // Ensure we can safely use gsl::narrow_cast<short>(...).
    if (sColumns <= 0 || sRows <= 0 || sColumns > SHRT_MAX || sRows > SHRT_MAX)
    {
        return false;
    }

    auto api = ServiceLocator::LocateGlobals().api;
    auto& screenInfo = _io.GetActiveOutputBuffer();

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    api->GetConsoleScreenBufferInfoExImpl(screenInfo, csbiex);

    const auto oldViewport = screenInfo.GetVirtualViewport();
    auto newViewport = Viewport::FromDimensions(oldViewport.Origin(), sColumns, sRows);
    // Always resize the width of the console
    csbiex.dwSize.X = gsl::narrow_cast<short>(sColumns);
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = gsl::narrow_cast<short>(sRows);
    }

    // If the cursor row is now past the bottom of the viewport, we'll have to
    // move the viewport down to bring it back into view. However, we don't want
    // to do this in pty mode, because the conpty resize operation is dependent
    // on the viewport *not* being adjusted.
    const auto cursorOverflow = csbiex.dwCursorPosition.Y - newViewport.BottomInclusive();
    if (cursorOverflow > 0 && !IsConsolePty())
    {
        newViewport = Viewport::Offset(newViewport, { 0, cursorOverflow });
    }

    // SetWindowInfo expect inclusive rects
    const auto sri = newViewport.ToInclusive();

    // SetConsoleScreenBufferInfoEx however expects exclusive rects
    const auto sre = newViewport.ToExclusive();
    csbiex.srWindow = til::unwrap_exclusive_small_rect(sre);

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
            changedRect.left,
            changedRect.top,
            changedRect.right - 1,
            changedRect.bottom - 1);
    }
}

void ConhostInternalGetSet::MarkPrompt(const Microsoft::Console::VirtualTerminal::DispatchTypes::ScrollMark& /*mark*/)
{
    // Not implemented for conhost.
}

void ConhostInternalGetSet::MarkCommandStart()
{
    // Not implemented for conhost.
}

void ConhostInternalGetSet::MarkOutputStart()
{
    // Not implemented for conhost.
}

void ConhostInternalGetSet::MarkCommandFinish(std::optional<unsigned int> /*error*/)
{
    // Not implemented for conhost.
}
