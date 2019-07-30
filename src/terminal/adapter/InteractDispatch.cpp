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
InteractDispatch::InteractDispatch(ConGetSet* const pConApi) :
    _pConApi(THROW_IF_NULL_ALLOC(pConApi))
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
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents)
{
    size_t dwWritten = 0;
    return !!_pConApi->PrivateWriteConsoleInputW(inputEvents, dwWritten);
}

// Method Description:
// - Writes a Ctrl-C event to the host. The host will then decide what to do
//      with it, including potentially sending an interrupt to a client
//      application.
// Arguments:
// <none>
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteCtrlC()
{
    KeyEvent key = KeyEvent(true, 1, 'C', 0, UNICODE_ETX, LEFT_CTRL_PRESSED);
    return !!_pConApi->PrivateWriteConsoleControlInput(key);
}

// Method Description:
// - Writes a string of input to the host. The string is converted to keystrokes
//      that will faithfully represent the input by CharToKeyEvents.
// Arguments:
// - pws: a string to write to the console.
// - cch: the number of chars in pws.
// Return Value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WriteString(_In_reads_(cch) const wchar_t* const pws,
                                   const size_t cch)
{
    if (cch == 0)
    {
        return true;
    }

    unsigned int codepage = 0;
    bool fSuccess = !!_pConApi->GetConsoleOutputCP(&codepage);
    if (fSuccess)
    {
        std::deque<std::unique_ptr<IInputEvent>> keyEvents;

        for (size_t i = 0; i < cch; ++i)
        {
            std::deque<std::unique_ptr<KeyEvent>> convertedEvents = CharToKeyEvents(pws[i], codepage);

            std::move(convertedEvents.begin(),
                      convertedEvents.end(),
                      std::back_inserter(keyEvents));
        }

        fSuccess = WriteInput(keyEvents);
    }
    return fSuccess;
}

//Method Description:
// Window Manipulation - Performs a variety of actions relating to the window,
//      such as moving the window position, resizing the window, querying
//      window state, forcing the window to repaint, etc.
//  This is kept seperate from the output version, as there may be
//      codes that are supported in one direction but not the other.
//Arguments:
// - uiFunction - An identifier of the WindowManipulation function to perform
// - rgusParams - Additional parameters to pass to the function
// - cParams - size of rgusParams
// Return value:
// True if handled successfully. False otherwise.
bool InteractDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType uiFunction,
                                          _In_reads_(cParams) const unsigned short* const rgusParams,
                                          const size_t cParams)
{
    bool fSuccess = false;
    // Other Window Manipulation functions:
    //  MSFT:13271098 - QueryViewport
    //  MSFT:13271146 - QueryScreenSize
    switch (uiFunction)
    {
    case DispatchTypes::WindowManipulationType::RefreshWindow:
        if (cParams == 0)
        {
            fSuccess = DispatchCommon::s_RefreshWindow(*_pConApi);
        }
        break;
    case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
        // TODO:GH#1765 We should introduce a better `ResizeConpty` function to
        // the ConGetSet interface, that specifically handles a conpty resize.
        if (cParams == 2)
        {
            fSuccess = DispatchCommon::s_ResizeWindow(*_pConApi, rgusParams[1], rgusParams[0]);
            if (fSuccess)
            {
                DispatchCommon::s_SuppressResizeRepaint(*_pConApi);
            }
        }
        break;
    default:
        fSuccess = false;
        break;
    }

    return fSuccess;
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
bool InteractDispatch::MoveCursor(const unsigned int row, const unsigned int col)
{
    unsigned int uiRow = row;
    unsigned int uiCol = col;

    bool fSuccess = true;
    // In VT, the origin is 1,1. For our array, it's 0,0. So subtract 1.
    if (row != 0)
    {
        uiRow = row - 1;
    }
    else
    {
        // The parser should never return 0 (0 maps to 1), so this is a failure condition.
        fSuccess = false;
    }

    if (col != 0)
    {
        uiCol = col - 1;
    }
    else
    {
        // The parser should never return 0 (0 maps to 1), so this is a failure condition.
        fSuccess = false;
    }

    if (fSuccess)
    {
        // First retrieve some information about the buffer
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        fSuccess = !!_pConApi->GetConsoleScreenBufferInfoEx(&csbiex);

        if (fSuccess)
        {
            COORD coordCursor = csbiex.dwCursorPosition;

            // Safely convert the UINT positions we were given into shorts (which is the size the console deals with)
            fSuccess = SUCCEEDED(UIntToShort(uiRow, &coordCursor.Y)) &&
                       SUCCEEDED(UIntToShort(uiCol, &coordCursor.X));

            if (fSuccess)
            {
                // Set the line and column values as offsets from the viewport edge. Use safe math to prevent overflow.
                fSuccess = SUCCEEDED(ShortAdd(coordCursor.Y, csbiex.srWindow.Top, &coordCursor.Y)) &&
                           SUCCEEDED(ShortAdd(coordCursor.X, csbiex.srWindow.Left, &coordCursor.X));

                if (fSuccess)
                {
                    // Apply boundary tests to ensure the cursor isn't outside the viewport rectangle.
                    coordCursor.Y = std::clamp(coordCursor.Y, csbiex.srWindow.Top, gsl::narrow<SHORT>(csbiex.srWindow.Bottom - 1));
                    coordCursor.X = std::clamp(coordCursor.X, csbiex.srWindow.Left, gsl::narrow<SHORT>(csbiex.srWindow.Right - 1));

                    // Finally, attempt to set the adjusted cursor position back into the console.
                    fSuccess = !!_pConApi->SetConsoleCursorPosition(coordCursor);
                }
            }
        }
    }

    return fSuccess;
}
