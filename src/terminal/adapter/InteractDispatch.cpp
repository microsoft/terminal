// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "InteractDispatch.hpp"
#include "DispatchCommon.hpp"
#include "conGetSet.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../../types/inc/convert.hpp"
#include "../../inc/unicode.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// takes ownership of pConApi
InteractDispatch::InteractDispatch(std::unique_ptr<ConGetSet> pConApi) :
    _pConApi(std::move(pConApi))
{
    THROW_HR_IF_NULL(E_INVALIDARG, _pConApi.get());
}

// Method Description:
// - Writes a collection of input to the host. The new input is appended to the
//      end of the input buffer.
//  If Ctrl+C is written with this function, it will not trigger a Ctrl-C
//      interrupt in the client, but instead write a Ctrl+C to the input buffer
//      to be read by the client.
// Arguments:
// - inputEvents: a collection of IInputEvents
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteInput(std::deque<std::unique_ptr<IInputEvent>>& inputEvents)
{
    size_t written = 0;
    return _pConApi->PrivateWriteConsoleInputW(inputEvents, written);
}

// Method Description:
// - Writes a key event to the host in a fashion that will enable the host to
//   process special keys such as Ctrl-C or Ctrl+Break. The host will then
//   decide what to do with it, including potentially sending an interrupt to a
//   client application.
// Arguments:
// - event: The key to send to the host.
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteCtrlKey(const KeyEvent& event)
{
    return _pConApi->PrivateWriteConsoleControlInput(event);
}

// Method Description:
// - Writes a string of input to the host. The string is converted to keystrokes
//      that will faithfully represent the input by CharToKeyEvents.
// Arguments:
// - string : a string to write to the console.
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteString(const std::wstring_view string)
{
    if (string.empty())
    {
        return true;
    }

    unsigned int codepage = 0;
    bool success = _pConApi->GetConsoleOutputCP(codepage);
    if (success)
    {
        std::deque<std::unique_ptr<IInputEvent>> keyEvents;

        for (const auto& wch : string)
        {
            std::deque<std::unique_ptr<KeyEvent>> convertedEvents = CharToKeyEvents(wch, codepage);

            std::move(convertedEvents.begin(),
                      convertedEvents.end(),
                      std::back_inserter(keyEvents));
        }

        success = WriteInput(keyEvents);
    }
    return success;
}

//Method Description:
// Window Manipulation - Performs a variety of actions relating to the window,
//      such as moving the window position, resizing the window, querying
//      window state, forcing the window to repaint, etc.
//  This is kept separate from the output version, as there may be
//      codes that are supported in one direction but not the other.
//Arguments:
// - function - An identifier of the WindowManipulation function to perform
// - parameters - Additional parameters to pass to the function
// Return value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                          const std::basic_string_view<size_t> parameters)
{
    bool success = false;
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize
    switch (function)
    {
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        if (parameters.empty())
        {
            success = DispatchCommon::s_RefreshWindow(*_pConApi);
        }
        break;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        // TODO:GH#1765 We should introduce a better `ResizeConpty` function to
        // the ConGetSet interface, that specifically handles a conpty resize.
        if (parameters.size() == 2)
        {
            success = DispatchCommon::s_ResizeWindow(*_pConApi, til::at(parameters, 1), til::at(parameters, 0));
            if (success)
            {
                DispatchCommon::s_SuppressResizeRepaint(*_pConApi);
            }
        }
        break;
    default:
        success = false;
        break;
    }

    return success;
}

//Method Description:
// Move Cursor: Moves the cursor to the provided VT coordinates. This is the
//      coordinate space where 1,1 is the top left cell of the viewport.
//Arguments:
// - row: The row to move the cursor to.
// - col: The column to move the cursor to.
// Return value:
// True if we successfully moved the cursor to the given location.
// False otherwise, including if given invalid coordinates (either component being 0)
//  or if any API calls failed.
bool InteractDispatch::MoveCursor(const size_t row, const size_t col)
{
    size_t rowFixed = row;
    size_t colFixed = col;

    bool success = true;
    // In VT, the origin is 1,1. For our array, it's 0,0. So subtract 1.
    if (row != 0)
    {
        rowFixed = row - 1;
    }
    else
    {
        // The parser should never return 0 (0 maps to 1), so this is a failure condition.
        success = false;
    }

    if (col != 0)
    {
        colFixed = col - 1;
    }
    else
    {
        // The parser should never return 0 (0 maps to 1), so this is a failure condition.
        success = false;
    }

    if (success)
    {
        // First retrieve some information about the buffer
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        success = _pConApi->GetConsoleScreenBufferInfoEx(csbiex);

        if (success)
        {
            COORD coordCursor = csbiex.dwCursorPosition;

            // Safely convert the size_t positions we were given into shorts (which is the size the console deals with)
            success = SUCCEEDED(SizeTToShort(rowFixed, &coordCursor.Y)) &&
                      SUCCEEDED(SizeTToShort(colFixed, &coordCursor.X));

            if (success)
            {
                // Set the line and column values as offsets from the viewport edge. Use safe math to prevent overflow.
                success = SUCCEEDED(ShortAdd(coordCursor.Y, csbiex.srWindow.Top, &coordCursor.Y)) &&
                          SUCCEEDED(ShortAdd(coordCursor.X, csbiex.srWindow.Left, &coordCursor.X));

                if (success)
                {
                    // Apply boundary tests to ensure the cursor isn't outside the viewport rectangle.
                    coordCursor.Y = std::clamp(coordCursor.Y, csbiex.srWindow.Top, gsl::narrow<SHORT>(csbiex.srWindow.Bottom - 1));
                    coordCursor.X = std::clamp(coordCursor.X, csbiex.srWindow.Left, gsl::narrow<SHORT>(csbiex.srWindow.Right - 1));

                    // Finally, attempt to set the adjusted cursor position back into the console.
                    success = _pConApi->SetConsoleCursorPosition(coordCursor);
                }
            }
        }
    }

    return success;
}

// Routine Description:
// - Checks if the InputBuffer is willing to accept VT Input directly
// Arguments:
// - <none>
// Return value:
// - true if enabled (see IsInVirtualTerminalInputMode). false otherwise.
bool InteractDispatch::IsVtInputEnabled() const
{
    return _pConApi->PrivateIsVtInputEnabled();
}
