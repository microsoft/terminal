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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // ConPTY should not respond to requests. That's the job of the terminal.
    if (gci.IsInVtIoMode())
    {
        return;
    }

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
// - Retrieves the text buffer and virtual viewport for the active output
//   buffer. Also returns a flag indicating whether it's the main buffer.
// Arguments:
// - <none>
// Return Value:
// - a tuple with the buffer reference, viewport, and main buffer flag.
ITerminalApi::BufferState ConhostInternalGetSet::GetBufferAndViewport()
{
    auto& info = _io.GetActiveOutputBuffer();
    return { info.GetTextBuffer(), info.GetVirtualViewport().ToExclusive(), info.Next == nullptr };
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
// - Sends the configured answerback message in response to an ENQ query.
// Return Value:
// - <none>
void ConhostInternalGetSet::ReturnAnswerback()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    ReturnResponse(gci.GetAnswerbackMessage());
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
    // ConPTY is supposed to be "transparent" to the VT application. Any VT it processes is given to the terminal.
    // As such, it must not react to this "CSI 1 t" or "CSI 2 t" sequence. That's the job of the terminal.
    // If the terminal encounters such a sequence, it can show/hide itself and let ConPTY know via its signal API.
    const auto window = ServiceLocator::LocateConsoleWindow();
    if (!window)
    {
        return;
    }

    // GH#13301 - When we send this ShowWindow message, if we send it to the
    // conhost HWND, it's going to need to get processed by the window message
    // thread before returning.
    // However, ShowWindowAsync doesn't have this problem. It'll post the
    // message to the window thread, then immediately return, so we don't have
    // to worry about deadlocking.
    const auto hwnd = window->GetWindowHandle();
    ::ShowWindowAsync(hwnd, showOrHide ? SW_SHOWNOACTIVATE : SW_MINIMIZE);
}

// Routine Description:
// - Set the code page used for translating text when calling A versions of I/O functions.
// Arguments:
// - codepage - the new code page of the console.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCodePage(const unsigned int codepage)
{
    LOG_IF_FAILED(DoSrvSetConsoleOutputCodePage(codepage));
    LOG_IF_FAILED(DoSrvSetConsoleInputCodePage(codepage));
}

// Routine Description:
// - Reset the code pages to their default values.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConhostInternalGetSet::ResetCodePage()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    LOG_IF_FAILED(DoSrvSetConsoleOutputCodePage(gci.DefaultOutputCP));
    LOG_IF_FAILED(DoSrvSetConsoleInputCodePage(gci.DefaultCP));
}

// Routine Description:
// - Gets the code page used for translating text when calling A versions of output functions.
// Arguments:
// - <none>
// Return Value:
// - the output code page of the console.
unsigned int ConhostInternalGetSet::GetOutputCodePage() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
}

// Routine Description:
// - Gets the code page used for translating text when calling A versions of input functions.
// Arguments:
// - <none>
// Return Value:
// - the input code page of the console.
unsigned int ConhostInternalGetSet::GetInputCodePage() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().CP;
}

// Routine Description:
// - Copies the given content to the clipboard.
// Arguments:
// - content - the text to be copied.
// Return Value:
// - <none>
void ConhostInternalGetSet::CopyToClipboard(const wil::zwstring_view /*content*/)
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
    const auto window = ServiceLocator::LocateConsoleWindow();
    if (!window)
    {
        return;
    }

    // Unlock the console, so the UI doesn't hang while we're busy.
    UnlockConsole();

    // This call will block for the duration, unless shutdown early.
    const auto windowHandle = window->GetWindowHandle();
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
    auto newViewport = Viewport::FromDimensions(oldViewport.Origin(), { sColumns, sRows });
    // Always resize the width of the console
    csbiex.dwSize.X = gsl::narrow_cast<short>(sColumns);
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = gsl::narrow_cast<short>(sRows);
    }

    // If the cursor row is now past the bottom of the viewport, we'll have to
    // move the viewport down to bring it back into view.
    const auto cursorOverflow = csbiex.dwCursorPosition.Y - newViewport.BottomInclusive();
    if (cursorOverflow > 0)
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

void ConhostInternalGetSet::InvokeCompletions(std::wstring_view /*menuJson*/, unsigned int /*replaceLength*/)
{
    // Not implemented for conhost.
}
void ConhostInternalGetSet::SearchMissingCommand(std::wstring_view /*missingCommand*/)
{
    // Not implemented for conhost.
}
