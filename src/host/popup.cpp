// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "popup.h"

#include "_output.h"
#include "output.h"

#include "dbcs.h"
#include "srvinit.h"
#include "stream.h"

#include "resource.h"

#include "utils.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;
// Routine Description:
// - Creates an object representing an interactive popup overlay during cooked mode command line editing.
// - NOTE: Modifies global popup count (and adjusts cursor visibility as appropriate.)
// Arguments:
// - screenInfo - Reference to screen on which the popup should be drawn/overlaid.
// - proposedSize - Suggested size of the popup. May be adjusted based on screen size.
Popup::Popup(SCREEN_INFORMATION& screenInfo, const til::size proposedSize) :
    _screenInfo(screenInfo),
    _userInputFunction(&Popup::_getUserInputInternal)
{
    _attributes = screenInfo.GetPopupAttributes();

    const auto size = _CalculateSize(screenInfo, proposedSize);
    const auto origin = _CalculateOrigin(screenInfo, size);

    _region.left = origin.x;
    _region.top = origin.y;
    _region.right = origin.x + size.width - 1;
    _region.bottom = origin.y + size.height - 1;

    _oldScreenSize = screenInfo.GetBufferSize().Dimensions();

    til::inclusive_rect TargetRect;
    TargetRect.left = 0;
    TargetRect.top = _region.top;
    TargetRect.right = _oldScreenSize.width - 1;
    TargetRect.bottom = _region.bottom;

    // copy the data into the backup buffer
    _oldContents = std::move(screenInfo.ReadRect(Viewport::FromInclusive(TargetRect)));

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto countWas = gci.PopupCount.fetch_add(1ui16);
    if (0 == countWas)
    {
        // If this is the first popup to be shown, stop the cursor from appearing/blinking
        screenInfo.GetTextBuffer().GetCursor().SetIsPopupShown(true);
    }
}

// Routine Description:
// - Cleans up a popup object
// - NOTE: Modifies global popup count (and adjusts cursor visibility as appropriate.)
Popup::~Popup()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto countWas = gci.PopupCount.fetch_sub(1);
    if (1 == countWas)
    {
        // Notify we're done showing popups.
        gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor().SetIsPopupShown(false);
    }
}

void Popup::Draw()
{
    _DrawBorder();
    _DrawContent();
}

// Routine Description:
// - Draws the outlines of the popup area in the screen buffer
void Popup::_DrawBorder()
{
    // fill attributes of top line
    til::point WriteCoord;
    WriteCoord.x = _region.left;
    WriteCoord.y = _region.top;
    _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

    // draw upper left corner
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_DOWN_AND_RIGHT, 1), WriteCoord);

    // draw upper bar
    WriteCoord.x += 1;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_HORIZONTAL, Width()), WriteCoord);

    // draw upper right corner
    WriteCoord.x = _region.right;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_DOWN_AND_LEFT, 1), WriteCoord);

    for (til::CoordType i = 0; i < Height(); i++)
    {
        WriteCoord.y += 1;
        WriteCoord.x = _region.left;

        // fill attributes
        _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

        _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_VERTICAL, 1), WriteCoord);

        WriteCoord.x = _region.right;
        _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_VERTICAL, 1), WriteCoord);
    }

    // Draw bottom line.
    // Fill attributes of top line.
    WriteCoord.x = _region.left;
    WriteCoord.y = _region.bottom;
    _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

    // Draw bottom left corner.
    WriteCoord.x = _region.left;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_UP_AND_RIGHT, 1), WriteCoord);

    // Draw lower bar.
    WriteCoord.x += 1;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_HORIZONTAL, Width()), WriteCoord);

    // draw lower right corner
    WriteCoord.x = _region.right;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_UP_AND_LEFT, 1), WriteCoord);
}

// Routine Description:
// - Draws prompt information in the popup area to tell the user what to enter.
// Arguments:
// - id - Resource ID for string to display to user
void Popup::_DrawPrompt(const UINT id)
{
    auto text = _LoadString(id);

    // Draw empty popup.
    til::point WriteCoord;
    WriteCoord.x = _region.left + 1;
    WriteCoord.y = _region.top + 1;
    auto lStringLength = Width();
    for (til::CoordType i = 0; i < Height(); i++)
    {
        const OutputCellIterator it(UNICODE_SPACE, _attributes, lStringLength);
        const auto done = _screenInfo.Write(it, WriteCoord);
        lStringLength = done.GetCellDistance(it);

        WriteCoord.y += 1;
    }

    WriteCoord.x = _region.left + 1;
    WriteCoord.y = _region.top + 1;

    // write prompt to screen
    lStringLength = gsl::narrow<til::CoordType>(text.size());
    if (lStringLength > Width())
    {
        text = text.substr(0, Width());
    }

    size_t used;
    LOG_IF_FAILED(ServiceLocator::LocateGlobals().api->WriteConsoleOutputCharacterWImpl(_screenInfo,
                                                                                        text,
                                                                                        WriteCoord,
                                                                                        used));
}

// Routine Description:
// - Cleans up a popup by restoring the stored buffer information to the region of
//   the screen that the popup was covering and frees resources.
void Popup::End()
{
    // restore previous contents to screen

    til::inclusive_rect SourceRect;
    SourceRect.left = 0;
    SourceRect.top = _region.top;
    SourceRect.right = _oldScreenSize.width - 1;
    SourceRect.bottom = _region.bottom;

    const auto sourceViewport = Viewport::FromInclusive(SourceRect);

    _screenInfo.WriteRect(_oldContents, sourceViewport.Origin());
}

// Routine Description:
// - Helper to calculate the size of the popup.
// Arguments:
// - screenInfo - Screen buffer we will be drawing into
// - proposedSize - The suggested size of the popup that may need to be adjusted to fit
// Return Value:
// - Coordinate size that the popup should consume in the screen buffer
til::size Popup::_CalculateSize(const SCREEN_INFORMATION& screenInfo, const til::size proposedSize)
{
    // determine popup dimensions
    auto size = proposedSize;
    size.width += 2; // add borders
    size.height += 2; // add borders

    const auto viewportSize = screenInfo.GetViewport().Dimensions();

    size.width = std::min(size.width, viewportSize.width);
    size.height = std::min(size.height, viewportSize.height);

    // make sure there's enough room for the popup borders
    THROW_HR_IF(E_NOT_SUFFICIENT_BUFFER, size.width < 2 || size.height < 2);

    return size;
}

// Routine Description:
// - Helper to calculate the origin point (within the screen buffer) for the popup
// Arguments:
// - screenInfo - Screen buffer we will be drawing into
// - size - The size that the popup will consume
// Return Value:
// - Coordinate position of the origin point of the popup
til::point Popup::_CalculateOrigin(const SCREEN_INFORMATION& screenInfo, const til::size size)
{
    const auto viewport = screenInfo.GetViewport();

    // determine origin.  center popup on window
    til::point origin;
    origin.x = (viewport.Width() - size.width) / 2 + viewport.Left();
    origin.y = (viewport.Height() - size.height) / 2 + viewport.Top();
    return origin;
}

// Routine Description:
// - Helper to return the width of the popup in columns
// Return Value:
// - Width of popup inside attached screen buffer.
til::CoordType Popup::Width() const noexcept
{
    return _region.right - _region.left - 1;
}

// Routine Description:
// - Helper to return the height of the popup in columns
// Return Value:
// - Height of popup inside attached screen buffer.
til::CoordType Popup::Height() const noexcept
{
    return _region.bottom - _region.top - 1;
}

// Routine Description:
// - Helper to get the position on top of some types of popup dialogs where
//   we should overlay the cursor for user input.
// Return Value:
// - Coordinate location on the popup where the cursor should be placed.
til::point Popup::GetCursorPosition() const noexcept
{
    til::point CursorPosition;
    CursorPosition.x = _region.right - MINIMUM_COMMAND_PROMPT_SIZE;
    CursorPosition.y = _region.top + 1;
    return CursorPosition;
}

// Routine Description:
// - changes the function used to gather user input. for allowing custom input during unit tests only
// Arguments:
// - function - function to use to fetch input
void Popup::SetUserInputFunction(UserInputFunction function) noexcept
{
    _userInputFunction = function;
}

// Routine Description:
// - gets a single char input from the user
// Arguments:
// - cookedReadData - cookedReadData for this popup operation
// - popupKey - on completion, will be true if key was a popup key
// - wch - on completion, the char read from the user
// Return Value:
// - relevant NTSTATUS
[[nodiscard]] NTSTATUS Popup::_getUserInput(COOKED_READ_DATA& cookedReadData, bool& popupKey, DWORD& modifiers, wchar_t& wch) noexcept
{
    return _userInputFunction(cookedReadData, popupKey, modifiers, wch);
}

// Routine Description:
// - gets a single char input from the user using the InputBuffer
// Arguments:
// - cookedReadData - cookedReadData for this popup operation
// - popupKey - on completion, will be true if key was a popup key
// - wch - on completion, the char read from the user
// Return Value:
// - relevant NTSTATUS
[[nodiscard]] NTSTATUS Popup::_getUserInputInternal(COOKED_READ_DATA& cookedReadData,
                                                    bool& popupKey,
                                                    DWORD& modifiers,
                                                    wchar_t& wch) noexcept
{
    const auto pInputBuffer = cookedReadData.GetInputBuffer();
    auto Status = GetChar(pInputBuffer,
                          &wch,
                          true,
                          nullptr,
                          &popupKey,
                          &modifiers);
    if (FAILED_NTSTATUS(Status) && Status != CONSOLE_STATUS_WAIT)
    {
        cookedReadData.BytesRead() = 0;
    }
    return Status;
}
