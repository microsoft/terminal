// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "cmdline.h"
#include "popup.h"
#include "CommandNumberPopup.hpp"
#include "CommandListPopup.hpp"
#include "CopyFromCharPopup.hpp"
#include "CopyToCharPopup.hpp"

#include "_output.h"
#include "output.h"
#include "stream.h"
#include "_stream.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "../types/inc/convert.hpp"
#include "srvinit.h"

#include "ApiRoutines.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

// Routine Description:
// - This routine is called when the user changes the screen/popup colors.
// - It goes through the popup structures and changes the saved contents to reflect the new screen/popup colors.
void CommandLine::UpdatePopups(const TextAttribute& NewAttributes,
                               const TextAttribute& NewPopupAttributes,
                               const TextAttribute& OldAttributes,
                               const TextAttribute& OldPopupAttributes)
{
    for (auto& popup : _popups)
    {
        try
        {
            popup->UpdateStoredColors(NewAttributes, NewPopupAttributes, OldAttributes, OldPopupAttributes);
        }
        CATCH_LOG();
    }
}

// Routine Description:
// - This routine validates a string buffer and returns the pointers of where the strings start within the buffer.
// Arguments:
// - Unicode - Supplies a boolean that is TRUE if the buffer contains Unicode strings, FALSE otherwise.
// - Buffer - Supplies the buffer to be validated.
// - Size - Supplies the size, in bytes, of the buffer to be validated.
// - Count - Supplies the expected number of strings in the buffer.
// ... - Supplies a pair of arguments per expected string. The first one is the expected size, in bytes, of the string
//       and the second one receives a pointer to where the string starts.
// Return Value:
// - TRUE if the buffer is valid, FALSE otherwise.
bool IsValidStringBuffer(_In_ bool Unicode, _In_reads_bytes_(Size) PVOID Buffer, _In_ ULONG Size, _In_ ULONG Count, ...)
{
    va_list Marker;
    va_start(Marker, Count);

    while (Count > 0)
    {
        ULONG const StringSize = va_arg(Marker, ULONG);
        PVOID* StringStart = va_arg(Marker, PVOID *);

        // Make sure the string fits in the supplied buffer and that it is properly aligned.
        if (StringSize > Size)
        {
            break;
        }

        if ((Unicode != false) && ((StringSize % sizeof(WCHAR)) != 0))
        {
            break;
        }

        *StringStart = Buffer;

        // Go to the next string.
        Buffer = RtlOffsetToPointer(Buffer, StringSize);
        Size -= StringSize;
        Count -= 1;
    }

    va_end(Marker);

    return Count == 0;
}

// Routine Description:
// - Detects Word delimiters
bool IsWordDelim(const wchar_t wch)
{
    // the space character is always a word delimiter. Do not add it to the WordDelimiters global because
    // that contains the user configurable word delimiters only.
    if (wch == UNICODE_SPACE)
    {
        return true;
    }
    const auto& delimiters = ServiceLocator::LocateGlobals().WordDelimiters;
    return std::find(delimiters.begin(), delimiters.end(), wch) != delimiters.end();
}

bool IsWordDelim(const std::wstring_view charData)
{
    if (charData.size() != 1)
    {
        return false;
    }
    return IsWordDelim(charData.front());
}

CommandLine::CommandLine() :
    _isVisible{ true }
{

}

CommandLine::~CommandLine()
{

}

CommandLine& CommandLine::Instance()
{
    static CommandLine c;
    return c;
}

bool CommandLine::IsEditLineEmpty() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    if (!gci.HasPendingCookedRead())
    {
        // If the cooked read data pointer is null, there is no edit line data and therefore it's empty.
        return true;
    }
    else if (0 == gci.CookedReadData().VisibleCharCount())
    {
        // If we had a valid pointer, but there are no visible characters for the edit line, then it's empty.
        // Someone started editing and back spaced the whole line out so it exists, but has no data.
        return true;
    }
    else
    {
        return false;
    }
}

void CommandLine::Hide(const bool fUpdateFields)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (!IsEditLineEmpty())
    {
        DeleteCommandLine(gci.CookedReadData(), fUpdateFields);
    }
    _isVisible = false;
}

void CommandLine::Show()
{
    _isVisible = true;
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (!IsEditLineEmpty())
    {
        RedrawCommandLine(gci.CookedReadData());
    }
}

// Routine Description:
// - Returns true if the commandline is currently being displayed. This is false
//      after Hide() is called, and before Show() is called again.
// Return Value:
// - true if the commandline should be displayed. Does not take into account
//   the echo state of the input. This is only controlled by calls to Hide/Show
bool CommandLine::IsVisible() const noexcept
{
    return _isVisible;
}

// Routine Description:
// - checks for the presence of a popup
// Return Value:
// - true if popup is present
bool CommandLine::HasPopup() const noexcept
{
    return !_popups.empty();
}

// Routine Description:
// - gets the topmost popup
// Arguments:
// Return Value:
// - ref to the topmost popup
Popup& CommandLine::GetPopup()
{
    return *_popups.front();
}

// Routine Description:
// - stops the current popup
void CommandLine::EndCurrentPopup()
{
    if (!_popups.empty())
    {
        _popups.front()->End();
        _popups.pop_front();
    }
}

// Routine Description:
// - stops all popups
void CommandLine::EndAllPopups()
{
    while (!_popups.empty())
    {
        EndCurrentPopup();
    }
}

void DeleteCommandLine(CookedRead& cookedReadData, const bool fUpdateFields)
{
    if (fUpdateFields)
    {
        cookedReadData.Erase();
    }
    else
    {
        cookedReadData.Hide();
    }
}

void RedrawCommandLine(CookedRead& cookedReadData)
{
    cookedReadData.Show();
}

// Routine Description:
// - This routine copies the commandline specified by Index into the cooked read buffer
void SetCurrentCommandLine(CookedRead& cookedReadData, _In_ SHORT Index) // index, not command number
{
    cookedReadData.SetPromptToCommand(gsl::narrow<size_t>(Index));
}

// Routine Description:
// - This routine handles the command list popup.  It puts up the popup, then calls ProcessCommandListInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
[[nodiscard]]
NTSTATUS CommandLine::_startCommandListPopup(CookedRead& cookedReadData)
{
    if (cookedReadData.HasHistory() &&
        cookedReadData.History().GetNumberOfCommands())
    {
        try
        {
            auto& popup = *_popups.emplace_front(std::make_unique<CommandListPopup>(cookedReadData.ScreenInfo(),
                                                                                    cookedReadData.History()));
            popup.Draw();
            return popup.Process(cookedReadData);
        }
        CATCH_RETURN();
    }
    else
    {
        return S_FALSE;
    }
}

// Routine Description:
// - This routine handles the "delete up to this char" popup.  It puts up the popup, then calls ProcessCopyFromCharInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
[[nodiscard]]
NTSTATUS CommandLine::_startCopyFromCharPopup(CookedRead& cookedReadData)
{
    // Delete the current command from cursor position to the
    // letter specified by the user. The user is prompted via
    // popup to enter a character.
    if (cookedReadData.HasHistory())
    {
        try
        {
            auto& popup = *_popups.emplace_front(std::make_unique<CopyFromCharPopup>(cookedReadData.ScreenInfo()));
            popup.Draw();
            return popup.Process(cookedReadData);
        }
        CATCH_RETURN();
    }
    else
    {
        return S_FALSE;
    }
}

// Routine Description:
// - This routine handles the "copy up to this char" popup.  It puts up the popup, then calls ProcessCopyToCharInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
// - S_FALSE - if we couldn't make a popup because we had no commands
[[nodiscard]]
NTSTATUS CommandLine::_startCopyToCharPopup(CookedRead& cookedReadData)
{
    // copy the previous command to the current command, up to but
    // not including the character specified by the user.  the user
    // is prompted via popup to enter a character.
    if (cookedReadData.HasHistory())
    {
        try
        {
            auto& popup = *_popups.emplace_front(std::make_unique<CopyToCharPopup>(cookedReadData.ScreenInfo()));
            popup.Draw();
            return popup.Process(cookedReadData);
        }
        CATCH_RETURN();
    }
    else
    {
        return S_FALSE;
    }
}

// Routine Description:
// - This routine handles the "enter command number" popup.  It puts up the popup, then calls ProcessCommandNumberInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
// - S_FALSE - if we couldn't make a popup because we had no commands or it wouldn't fit.
[[nodiscard]]
HRESULT CommandLine::StartCommandNumberPopup(CookedRead& cookedReadData)
{
    if (cookedReadData.HasHistory() &&
        cookedReadData.History().GetNumberOfCommands() &&
        cookedReadData.ScreenInfo().GetBufferSize().Width() >= MINIMUM_COMMAND_PROMPT_SIZE + 2)
    {
        try
        {
            auto& popup = *_popups.emplace_front(std::make_unique<CommandNumberPopup>(cookedReadData.ScreenInfo()));
            popup.Draw();

            // Save the original cursor position in case the user cancels out of the dialog
            //cookedReadData.BeforeDialogCursorPosition() = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();

            // Move the cursor into the dialog so the user can type multiple characters for the command number
            const COORD CursorPosition = popup.GetCursorPosition();
            LOG_IF_FAILED(cookedReadData.ScreenInfo().SetCursorPosition(CursorPosition, TRUE));

            // Transfer control to the handler routine
            return popup.Process(cookedReadData);
        }
        CATCH_RETURN();
    }
    else
    {
        return S_FALSE;
    }
}

// Routine Description:
// - Process virtual key code and updates the prompt line with the next history element in the direction
// specified by wch
// Arguments:
// - cookedReadData - The cooked read data to operate on
// - searchDirection - Direction in history to search
// Note:
// - May throw exceptions
void CommandLine::_processHistoryCycling(CookedRead& cookedReadData,
                                         const CommandHistory::SearchDirection searchDirection)
{
    // for doskey compatibility, buffer isn't circular. don't do anything if attempting
    // to cycle history past the bounds of the history buffer
    if (!cookedReadData.HasHistory())
    {
        return;
    }
    else if (searchDirection == CommandHistory::SearchDirection::Previous
             && cookedReadData.History().AtFirstCommand())
    {
        return;
    }
    else if (searchDirection == CommandHistory::SearchDirection::Next
             && cookedReadData.History().AtLastCommand())
    {
        return;
    }

    cookedReadData.SetPromptToCommand(searchDirection);
}

// Routine Description:
// - Sets the text on the prompt to the oldest run command in the cookedReadData's history
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Note:
// - May throw exceptions
void CommandLine::_setPromptToOldestCommand(CookedRead& cookedReadData)
{
    cookedReadData.SetPromptToOldestCommand();
}

// Routine Description:
// - Sets the text on the prompt the most recently run command in cookedReadData's history
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Note:
// - May throw exceptions
void CommandLine::_setPromptToNewestCommand(CookedRead& cookedReadData)
{
    cookedReadData.SetPromptToNewestCommand();
}

// Routine Description:
// - Deletes all prompt text to the right of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::DeletePromptAfterCursor(CookedRead& cookedReadData) noexcept
{
    cookedReadData.DeletePromptAfterInsertionIndex();
}

// Routine Description:
// - Deletes all user input on the prompt to the left of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
COORD CommandLine::_deletePromptBeforeCursor(CookedRead& cookedReadData) noexcept
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.DeletePromptBeforeInsertionIndex();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X -= gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the end of the prompt text
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
COORD CommandLine::_moveCursorToEndOfPrompt(CookedRead& cookedReadData) noexcept
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.MoveInsertionIndexToEnd();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X += gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the start of the user input on the prompt
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
COORD CommandLine::_moveCursorToStartOfPrompt(CookedRead& cookedReadData) noexcept
{
    cookedReadData.MoveInsertionIndexToStart();
    return cookedReadData.PromptStartLocation();
}

// Routine Description:
// - Moves the cursor left by a word
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - New cursor position
COORD CommandLine::_moveCursorLeftByWord(CookedRead& cookedReadData) noexcept
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.MoveInsertionIndexLeftByWord();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X -= gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Moves cursor left by a glyph
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - New cursor position
COORD CommandLine::_moveCursorLeft(CookedRead& cookedReadData)
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.MoveInsertionIndexLeft();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X -= gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the right by a word
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
COORD CommandLine::_moveCursorRightByWord(CookedRead& cookedReadData) noexcept
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.MoveInsertionIndexRightByWord();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X += gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the right by a glyph
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
COORD CommandLine::_moveCursorRight(CookedRead& cookedReadData) noexcept
{
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const size_t cellsMoved = cookedReadData.MoveInsertionIndexRight();
    // the cursor is adjusted to be within the bounds of the screen later, don't need to worry about it here
    cursorPosition.X += gsl::narrow<short>(cellsMoved);
    return cursorPosition;
}

// Routine Description:
// - Place a ctrl-z in the current command line
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_insertCtrlZ(CookedRead& cookedReadData) noexcept
{
    cookedReadData.InsertCtrlZ();
}

// Routine Description:
// - Empties the command history for cookedReadData
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_deleteCommandHistory(CookedRead& cookedReadData) noexcept
{
    if (cookedReadData.HasHistory())
    {
        cookedReadData.History().Empty();
        cookedReadData.History().Flags |= CLE_ALLOCATED;
    }
}

// Routine Description:
// - Copy the remainder of the previous command to the current command.
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_fillPromptWithPreviousCommandFragment(CookedRead& cookedReadData) noexcept
{
    cookedReadData.FillPromptWithPreviousCommandFragment();
}

// Routine Description:
// - Cycles through the stored commands that start with the characters in the current command.
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
void CommandLine::_cycleMatchingCommandHistoryToPrompt(CookedRead& cookedReadData)
{
    cookedReadData.SetPromptToMatchingHistoryCommand();
}

// Routine Description:
// - Deletes a glyph from the right side of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
void CommandLine::DeleteFromRightOfCursor(CookedRead& cookedReadData) noexcept
{
    cookedReadData.DeleteFromRightOfInsertionIndex();
}

// TODO: [MSFT:4586207] Clean up this mess -- needs helpers. http://osgvsowi/4586207
// Routine Description:
// - This routine process command line editing keys.
// Return Value:
// - CONSOLE_STATUS_WAIT - CommandListPopup ran out of input
// - CONSOLE_STATUS_READ_COMPLETE - user hit <enter> in CommandListPopup
// - STATUS_SUCCESS - everything's cool
[[nodiscard]]
NTSTATUS CommandLine::ProcessCommandLine(CookedRead& cookedReadData,
                                         _In_ WCHAR wch,
                                         const DWORD dwKeyState)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    COORD cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    NTSTATUS Status;

    const bool altPressed = WI_IsAnyFlagSet(dwKeyState, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
    const bool ctrlPressed = WI_IsAnyFlagSet(dwKeyState, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
    bool UpdateCursorPosition = false;
    switch (wch)
    {
    case VK_ESCAPE:
        DeleteCommandLine(cookedReadData, true);
        break;
    case VK_DOWN:
        try
        {
            _processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
            Status = STATUS_SUCCESS;
        }
        catch (...)
        {
            Status = wil::ResultFromCaughtException();
        }
        break;
    case VK_UP:
    case VK_F5:
        try
        {
            _processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
            Status = STATUS_SUCCESS;
        }
        catch (...)
        {
            Status = wil::ResultFromCaughtException();
        }
        break;
    case VK_PRIOR:
        try
        {
            _setPromptToOldestCommand(cookedReadData);
            Status = STATUS_SUCCESS;
        }
        catch (...)
        {
            Status = wil::ResultFromCaughtException();
        }
        break;
    case VK_NEXT:
        try
        {
            _setPromptToNewestCommand(cookedReadData);
            Status = STATUS_SUCCESS;
        }
        catch (...)
        {
            Status = wil::ResultFromCaughtException();
        }
        break;
    case VK_END:
        if (ctrlPressed)
        {
            DeletePromptAfterCursor(cookedReadData);
        }
        else
        {
            cursorPosition = _moveCursorToEndOfPrompt(cookedReadData);
            UpdateCursorPosition = true;
        }
        break;
    case VK_HOME:
        if (ctrlPressed)
        {
            cursorPosition = _deletePromptBeforeCursor(cookedReadData);
            UpdateCursorPosition = true;
        }
        else
        {
            cursorPosition = _moveCursorToStartOfPrompt(cookedReadData);
            UpdateCursorPosition = true;
        }
        break;
    case VK_LEFT:
        if (ctrlPressed)
        {
            cursorPosition = _moveCursorLeftByWord(cookedReadData);
            UpdateCursorPosition = true;
        }
        else
        {
            cursorPosition = _moveCursorLeft(cookedReadData);
            UpdateCursorPosition = true;
        }
        break;
    case VK_F1:
    {
        // we don't need to check for end of buffer here because we've
        // already done it.
        cursorPosition = _moveCursorRight(cookedReadData);
        UpdateCursorPosition = true;
        break;
    }
    case VK_RIGHT:
        // we don't need to check for end of buffer here because we've
        // already done it.
        if (ctrlPressed)
        {
            cursorPosition = _moveCursorRightByWord(cookedReadData);
            UpdateCursorPosition = true;
        }
        else
        {
            cursorPosition = _moveCursorRight(cookedReadData);
            UpdateCursorPosition = true;
        }
        break;
    case VK_F2:
    {
        Status = _startCopyToCharPopup(cookedReadData);
        if (S_FALSE == Status)
        {
            // We couldn't make the popup, so loop around and read the next character.
            break;
        }
        else
        {
            return Status;
        }
    }
    case VK_F3:
        _fillPromptWithPreviousCommandFragment(cookedReadData);
        break;
    case VK_F4:
    {
        Status = _startCopyFromCharPopup(cookedReadData);
        if (S_FALSE == Status)
        {
            // We couldn't display a popup. Go around a loop behind.
            break;
        }
        else
        {
            return Status;
        }
    }
    case VK_F6:
    {
        _insertCtrlZ(cookedReadData);
        break;
    }
    case VK_F7:
        if (!ctrlPressed && !altPressed)
        {
            Status = _startCommandListPopup(cookedReadData);
            return Status;
        }
        else if (altPressed)
        {
            _deleteCommandHistory(cookedReadData);
        }
        break;

    case VK_F8:
        try
        {
            _cycleMatchingCommandHistoryToPrompt(cookedReadData);
        }
        catch (...)
        {
            Status = wil::ResultFromCaughtException();
        }
        break;
    case VK_F9:
    {
        Status = StartCommandNumberPopup(cookedReadData);
        if (S_FALSE == Status)
        {
            // If we couldn't make the popup, break and go around to read another input character.
            break;
        }
        else
        {
            return Status;
        }
    }
    case VK_F10:
        // Alt+F10 clears the aliases for specifically cmd.exe.
        if (altPressed)
        {
            Alias::s_ClearCmdExeAliases();
        }
        break;
    case VK_INSERT:
        cookedReadData.SetInsertMode(!cookedReadData.IsInsertMode());
        cookedReadData.ScreenInfo().SetCursorDBMode(cookedReadData.IsInsertMode() != gci.GetInsertMode());
        break;
    case VK_DELETE:
        DeleteFromRightOfCursor(cookedReadData);
        break;
    default:
        FAIL_FAST_HR(E_NOTIMPL);
        break;
    }

    if (UpdateCursorPosition && cookedReadData.IsEchoInput())
    {
        Status = AdjustCursorPosition(cookedReadData.ScreenInfo(), cursorPosition, true, nullptr);
        FAIL_FAST_IF_NTSTATUS_FAILED(Status);
    }

    return STATUS_SUCCESS;
}
