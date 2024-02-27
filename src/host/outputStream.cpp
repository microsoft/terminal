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
    // TODO GH#4954 During the input refactor we may want to add a "priority" input list
    // to make sure that "response" input is spooled directly into the application.
    // We switched this to an append (vs. a prepend) to fix GH#1637, a bug where two CPR
    // could collide with each other.
    _io.GetActiveInputBuffer()->WriteString(response);
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
    THROW_IF_FAILED(info.SetViewportOrigin(true, position, true));
    // SetViewportOrigin() only updates the virtual bottom (the bottom coordinate of the area
    // in the text buffer a VT client writes its output into) when it's moving downwards.
    // But this function is meant to truly move the viewport no matter what. Otherwise `tput reset` breaks.
    info.UpdateBottom();
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
// - Sets the state of one of the system modes.
// Arguments:
// - mode - The mode being updated.
// - enabled - True to enable the mode, false to disable it.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetSystemMode(const Mode mode, const bool enabled)
{
    switch (mode)
    {
    case Mode::AutoWrap:
        WI_UpdateFlag(_io.GetActiveOutputBuffer().OutputMode, ENABLE_WRAP_AT_EOL_OUTPUT, enabled);
        break;
    case Mode::LineFeed:
        WI_UpdateFlag(_io.GetActiveOutputBuffer().OutputMode, DISABLE_NEWLINE_AUTO_RETURN, !enabled);
        break;
    case Mode::BracketedPaste:
        ServiceLocator::LocateGlobals().getConsoleInformation().SetBracketedPasteMode(enabled);
        break;
    default:
        THROW_HR(E_INVALIDARG);
    }
}

// Routine Description:
// - Retrieves the current state of one of the system modes.
// Arguments:
// - mode - The mode being queried.
// Return Value:
// - true if the mode is enabled. false otherwise.
bool ConhostInternalGetSet::GetSystemMode(const Mode mode) const
{
    switch (mode)
    {
    case Mode::AutoWrap:
        return WI_IsFlagSet(_io.GetActiveOutputBuffer().OutputMode, ENABLE_WRAP_AT_EOL_OUTPUT);
    case Mode::LineFeed:
        return WI_IsFlagClear(_io.GetActiveOutputBuffer().OutputMode, DISABLE_NEWLINE_AUTO_RETURN);
    case Mode::BracketedPaste:
        return ServiceLocator::LocateGlobals().getConsoleInformation().GetBracketedPasteMode();
    default:
        THROW_HR(E_INVALIDARG);
    }
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
// - title - The string to set as the window title
// Return Value:
// - <none>
void ConhostInternalGetSet::SetWindowTitle(std::wstring_view title)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.SetTitle(title.empty() ? gci.GetOriginalTitle() : title);
}

// Routine Description:
// - Swaps to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. This creates a new alternate, and switches to it.
//     If there is an already existing alternate, it is discarded.
// Arguments:
// - attrs - the attributes the buffer is initialized with.
// Return Value:
// - <none>
void ConhostInternalGetSet::UseAlternateScreenBuffer(const TextAttribute& attrs)
{
    THROW_IF_NTSTATUS_FAILED(_io.GetActiveOutputBuffer().UseAlternateScreenBuffer(attrs));
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

    // We need to save the attributes separately, since the wAttributes field in
    // CONSOLE_SCREEN_BUFFER_INFOEX is not capable of representing the extended
    // attribute values, and can end up corrupting that data when restored.
    const auto attributes = screenInfo.GetTextBuffer().GetCurrentAttributes();
    const auto restoreAttributes = wil::scope_exit([&] {
        screenInfo.GetTextBuffer().SetCurrentAttributes(attributes);
    });

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
    if (screenInfo.HasAccessibilityEventing() && changedRect)
    {
        screenInfo.NotifyAccessibilityEventing(
            changedRect.left,
            changedRect.top,
            changedRect.right - 1,
            changedRect.bottom - 1);
    }
}

// Routine Description:
// - Implements conhost-specific behavior when the buffer is rotated.
// Arguments:
// - delta - the number of cycles that the buffer has rotated.
// Return value:
// - <none>
void ConhostInternalGetSet::NotifyBufferRotation(const int delta)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    if (screenInfo.IsActiveScreenBuffer())
    {
        auto pNotifier = ServiceLocator::LocateAccessibilityNotifier();
        if (pNotifier)
        {
            pNotifier->NotifyConsoleUpdateScrollEvent(0, -delta);
        }
    }
}

void ConhostInternalGetSet::MarkPrompt(const ::ScrollMark& /*mark*/)
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
void ConhostInternalGetSet::InvokeCompletions(std::wstring_view /*menuJson*/, unsigned int /*replaceLength*/)
{
    // Not implemented for conhost.
}
