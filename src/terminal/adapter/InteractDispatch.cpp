// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

// Some of the interactivity classes pulled in by ServiceLocator.hpp are not
// yet audit-safe, so for now we'll need to disable a bunch of warnings.
#pragma warning(disable : 26432)
#pragma warning(disable : 26440)
#pragma warning(disable : 26455)

// We end up including ApiMessage.h somehow, which uses nameless unions
#pragma warning(disable : 4201)

#include "InteractDispatch.hpp"
#include "../../host/conddkrefs.h"
#include "../../interactivity/inc/ServiceLocator.hpp"
#include "../../interactivity/inc/EventSynthesis.hpp"
#include "../../types/inc/Viewport.hpp"

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

InteractDispatch::InteractDispatch() :
    _api{ ServiceLocator::LocateGlobals().getConsoleInformation() }
{
}

// Method Description:
// - Writes a collection of input to the host. The new input is appended to the
//      end of the input buffer.
//  If Ctrl+C is written with this function, it will not trigger a Ctrl-C
//      interrupt in the client, but instead write a Ctrl+C to the input buffer
//      to be read by the client.
// Arguments:
// - inputEvents: a collection of IInputEvents
void InteractDispatch::WriteInput(const std::span<const INPUT_RECORD>& inputEvents)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.GetActiveInputBuffer()->Write(inputEvents);
}

// Method Description:
// - Writes a key event to the host in a fashion that will enable the host to
//   process special keys such as Ctrl-C or Ctrl+Break. The host will then
//   decide what to do with it, including potentially sending an interrupt to a
//   client application.
// Arguments:
// - event: The key to send to the host.
void InteractDispatch::WriteCtrlKey(const INPUT_RECORD& event)
{
    HandleGenericKeyEvent(event, false);
}

// Call this method to write some plain text to the InputBuffer.
//
// Since the hosting terminal for ConPTY may not support win32-input-mode,
// it may send an "A" key as an "A", for which we need to generate up/down events.
// Because of this, we cannot simply call InputBuffer::WriteString directly.
void InteractDispatch::WriteString(const std::wstring_view string)
{
    if (!string.empty())
    {
        const auto codepage = _api.GetOutputCodePage();
        InputEventQueue keyEvents;

        for (const auto& wch : string)
        {
            CharToKeyEvents(wch, codepage, keyEvents);
        }

        WriteInput(keyEvents);
    }
}

void InteractDispatch::WriteStringRaw(std::wstring_view string)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.GetActiveInputBuffer()->WriteString(string);
}

//Method Description:
// Window Manipulation - Performs a variety of actions relating to the window,
//      such as moving the window position, resizing the window, querying
//      window state, forcing the window to repaint, etc.
//  This is kept separate from the output version, as there may be
//      codes that are supported in one direction but not the other.
//Arguments:
// - function - An identifier of the WindowManipulation function to perform
// - parameter1 - The first optional parameter for the function
// - parameter2 - The second optional parameter for the function
void InteractDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                          const VTParameter parameter1,
                                          const VTParameter parameter2)
{
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize
    switch (function)
    {
    case DispatchTypes::WindowManipulationType::DeIconifyWindow:
        _api.ShowWindow(true);
        break;
    case DispatchTypes::WindowManipulationType::IconifyWindow:
        _api.ShowWindow(false);
        break;
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        _api.GetBufferAndViewport().buffer.TriggerRedrawAll();
        break;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        // TODO:GH#1765 We should introduce a better `ResizeConpty` function to
        // ConhostInternalGetSet, that specifically handles a conpty resize.
        _api.ResizeWindow(parameter2.value_or(0), parameter1.value_or(0));
        break;
    default:
        break;
    }
}

//Method Description:
// Move Cursor: Moves the cursor to the provided VT coordinates. This is the
//      coordinate space where 1,1 is the top left cell of the viewport.
//Arguments:
// - row: The row to move the cursor to.
// - col: The column to move the cursor to.
void InteractDispatch::MoveCursor(const VTInt row, const VTInt col)
{
    // First retrieve some information about the buffer
    const auto viewport = _api.GetBufferAndViewport().viewport;

    // In VT, the origin is 1,1. For our array, it's 0,0. So subtract 1.
    // Apply boundary tests to ensure the cursor isn't outside the viewport rectangle.
    til::point coordCursor{ col - 1 + viewport.left, row - 1 + viewport.top };
    coordCursor.y = std::clamp(coordCursor.y, viewport.top, viewport.bottom);
    coordCursor.x = std::clamp(coordCursor.x, viewport.left, viewport.right);

    // Finally, attempt to set the adjusted cursor position back into the console.
    const auto api = gsl::not_null{ ServiceLocator::LocateGlobals().api };
    auto& info = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer();
    LOG_IF_FAILED(api->SetConsoleCursorPositionImpl(info, coordCursor));
}

// Routine Description:
// - Checks if the InputBuffer is willing to accept VT Input directly
// Arguments:
// - <none>
// Return value:
// - true if enabled (see IsInVirtualTerminalInputMode). false otherwise.
bool InteractDispatch::IsVtInputEnabled() const
{
    return _api.IsVtInputEnabled();
}

// Method Description:
// - Inform the console that the window is focused. This is used by ConPTY.
//   Terminals can send ConPTY a FocusIn/FocusOut sequence on the input pipe,
//   which will end up here. This will update the console's internal tracker if
//   it's focused or not, as to match the end-terminal's state.
// - Used to call ConsoleControl(ConsoleSetForeground,...).
// Arguments:
// - focused: if the terminal is now focused
void InteractDispatch::FocusChanged(const bool focused)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();

    // This should likely always be true - we shouldn't ever have an
    // InteractDispatch outside ConPTY mode, but just in case...
    if (gci.IsInVtIoMode())
    {
        WI_UpdateFlag(gci.Flags, CONSOLE_HAS_FOCUS, focused);
        gci.ProcessHandleList.ModifyConsoleProcessFocus(focused);
        gci.pInputBuffer->WriteFocusEvent(focused);
    }
    // Does nothing outside of ConPTY. If there's a real HWND, then the HWND is solely in charge.
}
