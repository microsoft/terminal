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

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;
// Routine Description:
// - Creates an object representing an interactive popup overlay during cooked mode command line editing.
// - NOTE: Modifies global popup count (and adjusts cursor visibility as appropriate.)
// Arguments:
// - screenInfo - Reference to screen on which the popup should be drawn/overlayed.
// - proposedSize - Suggested size of the popup. May be adjusted based on screen size.
Popup::Popup(SCREEN_INFORMATION& screenInfo, const COORD proposedSize) :
    _screenInfo(screenInfo),
    _userInputFunction(&Popup::_getUserInputInternal)
{
    _attributes = screenInfo.GetPopupAttributes()->GetLegacyAttributes();

    const COORD size = _CalculateSize(screenInfo, proposedSize);
    const COORD origin = _CalculateOrigin(screenInfo, size);

    _region.Left = origin.X;
    _region.Top = origin.Y;
    _region.Right = gsl::narrow<SHORT>(origin.X + size.X - 1i16);
    _region.Bottom = gsl::narrow<SHORT>(origin.Y + size.Y - 1i16);

    _oldScreenSize = screenInfo.GetBufferSize().Dimensions();

    SMALL_RECT TargetRect;
    TargetRect.Left = 0;
    TargetRect.Top = _region.Top;
    TargetRect.Right = _oldScreenSize.X - 1;
    TargetRect.Bottom = _region.Bottom;

    // copy the data into the backup buffer
    _oldContents = std::move(screenInfo.ReadRect(Viewport::FromInclusive(TargetRect)));

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto countWas = gci.PopupCount.fetch_sub(1i16);
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
    COORD WriteCoord;
    WriteCoord.X = _region.Left;
    WriteCoord.Y = _region.Top;
    _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

    // draw upper left corner
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_DOWN_AND_RIGHT, 1), WriteCoord);

    // draw upper bar
    WriteCoord.X += 1;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_HORIZONTAL, Width()), WriteCoord);

    // draw upper right corner
    WriteCoord.X = _region.Right;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_DOWN_AND_LEFT, 1), WriteCoord);

    for (SHORT i = 0; i < Height(); i++)
    {
        WriteCoord.Y += 1;
        WriteCoord.X = _region.Left;

        // fill attributes
        _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

        _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_VERTICAL, 1), WriteCoord);

        WriteCoord.X = _region.Right;
        _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_VERTICAL, 1), WriteCoord);
    }

    // Draw bottom line.
    // Fill attributes of top line.
    WriteCoord.X = _region.Left;
    WriteCoord.Y = _region.Bottom;
    _screenInfo.Write(OutputCellIterator(_attributes, Width() + 2), WriteCoord);

    // Draw bottom left corner.
    WriteCoord.X = _region.Left;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_UP_AND_RIGHT, 1), WriteCoord);

    // Draw lower bar.
    WriteCoord.X += 1;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_HORIZONTAL, Width()), WriteCoord);

    // draw lower right corner
    WriteCoord.X = _region.Right;
    _screenInfo.Write(OutputCellIterator(UNICODE_BOX_DRAW_LIGHT_UP_AND_LEFT, 1), WriteCoord);
}

// Routine Description:
// - Draws prompt information in the popup area to tell the user what to enter.
// Arguments:
// - id - Resource ID for string to display to user
void Popup::_DrawPrompt(const UINT id)
{
    std::wstring text = _LoadString(id);

    // Draw empty popup.
    COORD WriteCoord;
    WriteCoord.X = _region.Left + 1i16;
    WriteCoord.Y = _region.Top + 1i16;
    size_t lStringLength = Width();
    for (SHORT i = 0; i < Height(); i++)
    {
        const OutputCellIterator it(UNICODE_SPACE, _attributes, lStringLength);
        const auto done = _screenInfo.Write(it, WriteCoord);
        lStringLength = done.GetCellDistance(it);

        WriteCoord.Y += 1;
    }

    WriteCoord.X = _region.Left + 1i16;
    WriteCoord.Y = _region.Top + 1i16;

    // write prompt to screen
    lStringLength = text.size();
    if (lStringLength > (ULONG)Width())
    {
        text = text.substr(0, Width());
    }

    size_t used;
    LOG_IF_FAILED(ServiceLocator::LocateGlobals().api.WriteConsoleOutputCharacterWImpl(_screenInfo,
                                                                                       text,
                                                                                       WriteCoord,
                                                                                       used));
}

// Routine Description:
// - Updates the colors of the backed up information inside this popup.
// - This is useful if user preferences change while a popup is displayed.
// Arguments:
// - newAttr - The new default color for text in the buffer
// - newPopupAttr - The new color for text in popups
// - oldAttr - The previous default color for text in the buffer
// - oldPopupAttr - The previous color for text in popups
void Popup::UpdateStoredColors(const TextAttribute& newAttr,
                               const TextAttribute& newPopupAttr,
                               const TextAttribute& oldAttr,
                               const TextAttribute& oldPopupAttr)
{
    // We also want to find and replace the inversion of the popup colors in case there are highlights
    const WORD wOldPopupLegacy = oldPopupAttr.GetLegacyAttributes();
    const WORD wNewPopupLegacy = newPopupAttr.GetLegacyAttributes();

    const WORD wOldPopupAttrInv = (WORD)(((wOldPopupLegacy << 4) & 0xf0) | ((wOldPopupLegacy >> 4) & 0x0f));
    const WORD wNewPopupAttrInv = (WORD)(((wNewPopupLegacy << 4) & 0xf0) | ((wNewPopupLegacy >> 4) & 0x0f));

    const TextAttribute oldPopupInv{ wOldPopupAttrInv };
    const TextAttribute newPopupInv{ wNewPopupAttrInv };

    // Walk through every row in the rectangle
    for (size_t i = 0; i < _oldContents.Height(); i++)
    {
        auto row = _oldContents.GetRow(i);

        // Walk through every cell
        for (auto& cell : row)
        {
            auto& attr = cell.TextAttr();

            if (attr == oldAttr)
            {
                attr = newAttr;
            }
            else if (attr == oldPopupAttr)
            {
                attr = newPopupAttr;
            }
            else if (attr == oldPopupInv)
            {
                attr = newPopupInv;
            }
        }
    }
}

// Routine Description:
// - Cleans up a popup by restoring the stored buffer information to the region of
//   the screen that the popup was covering and frees resources.
void Popup::End()
{
    // restore previous contents to screen

    SMALL_RECT SourceRect;
    SourceRect.Left = 0i16;
    SourceRect.Top = _region.Top;
    SourceRect.Right = _oldScreenSize.X - 1i16;
    SourceRect.Bottom = _region.Bottom;

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
COORD Popup::_CalculateSize(const SCREEN_INFORMATION& screenInfo, const COORD proposedSize)
{
    // determine popup dimensions
    COORD size = proposedSize;
    size.X += 2; // add borders
    size.Y += 2; // add borders

    const COORD viewportSize = screenInfo.GetViewport().Dimensions();

    size.X = std::min(size.X, viewportSize.X);
    size.Y = std::min(size.Y, viewportSize.Y);

    // make sure there's enough room for the popup borders
    THROW_HR_IF(E_NOT_SUFFICIENT_BUFFER, size.X < 2 || size.Y < 2);

    return size;
}

// Routine Description:
// - Helper to calculate the origin point (within the screen buffer) for the popup
// Arguments:
// - screenInfo - Screen buffer we will be drawing into
// - size - The size that the popup will consume
// Return Value:
// - Coordinate position of the origin point of the popup
COORD Popup::_CalculateOrigin(const SCREEN_INFORMATION& screenInfo, const COORD size)
{
    const auto viewport = screenInfo.GetViewport();

    // determine origin.  center popup on window
    COORD origin;
    origin.X = gsl::narrow<SHORT>((viewport.Width() - size.X) / 2 + viewport.Left());
    origin.Y = gsl::narrow<SHORT>((viewport.Height() - size.Y) / 2 + viewport.Top());
    return origin;
}

// Routine Description:
// - Helper to return the width of the popup in columns
// Return Value:
// - Width of popup inside attached screen buffer.
SHORT Popup::Width() const noexcept
{
    return _region.Right - _region.Left - 1i16;
}

// Routine Description:
// - Helper to return the height of the popup in columns
// Return Value:
// - Height of popup inside attached screen buffer.
SHORT Popup::Height() const noexcept
{
    return _region.Bottom - _region.Top - 1i16;
}

// Routine Description:
// - Helper to get the position on top of some types of popup dialogs where
//   we should overlay the cursor for user input.
// Return Value:
// - Coordinate location on the popup where the cursor should be placed.
COORD Popup::GetCursorPosition() const noexcept
{
    COORD CursorPosition;
    CursorPosition.X = _region.Right - static_cast<SHORT>(MINIMUM_COMMAND_PROMPT_SIZE);
    CursorPosition.Y = _region.Top + 1i16;
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
    InputBuffer* const pInputBuffer = cookedReadData.GetInputBuffer();
    NTSTATUS Status = GetChar(pInputBuffer,
                              &wch,
                              true,
                              nullptr,
                              &popupKey,
                              &modifiers);
    if (!NT_SUCCESS(Status) && Status != CONSOLE_STATUS_WAIT)
    {
        cookedReadData.BytesRead() = 0;
    }
    return Status;
}
