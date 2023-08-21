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

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop
using Microsoft::Console::Interactivity::ServiceLocator;

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
    return std::ranges::find(delimiters, wch) != delimiters.end();
}

bool IsWordDelim(const std::wstring_view charData)
{
    return charData.size() == 1 && IsWordDelim(charData.front());
}

CommandLine::CommandLine() :
    _isVisible{ true }
{
}

CommandLine::~CommandLine() = default;

CommandLine& CommandLine::Instance()
{
    static CommandLine c;
    return c;
}

bool CommandLine::IsEditLineEmpty()
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (!IsEditLineEmpty())
    {
        DeleteCommandLine(gci.CookedReadData(), fUpdateFields);
    }
    _isVisible = false;
}

void CommandLine::Show()
{
    _isVisible = true;
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
Popup& CommandLine::GetPopup() const
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
        _popups.front()->End();
        _popups.pop_front();
    }
}

void DeleteCommandLine(COOKED_READ_DATA& cookedReadData, const bool fUpdateFields)
{
    auto CharsToWrite = cookedReadData.VisibleCharCount();
    auto coordOriginalCursor = cookedReadData.OriginalCursorPosition();
    const auto coordBufferSize = cookedReadData.ScreenInfo().GetBufferSize().Dimensions();

    // catch the case where the current command has scrolled off the top of the screen.
    if (coordOriginalCursor.y < 0)
    {
        CharsToWrite += coordBufferSize.width * coordOriginalCursor.y;
        CharsToWrite += cookedReadData.OriginalCursorPosition().x; // account for prompt
        cookedReadData.OriginalCursorPosition().x = 0;
        cookedReadData.OriginalCursorPosition().y = 0;
        coordOriginalCursor.x = 0;
        coordOriginalCursor.y = 0;
    }

    if (!CheckBisectStringW(cookedReadData.BufferStartPtr(),
                            CharsToWrite,
                            coordBufferSize.width - cookedReadData.OriginalCursorPosition().x))
    {
        CharsToWrite++;
    }

    try
    {
        cookedReadData.ScreenInfo().Write(OutputCellIterator(UNICODE_SPACE, CharsToWrite), coordOriginalCursor);
    }
    CATCH_LOG();

    if (fUpdateFields)
    {
        cookedReadData.Erase();
    }

    LOG_IF_FAILED(cookedReadData.ScreenInfo().SetCursorPosition(cookedReadData.OriginalCursorPosition(), true));
}

void RedrawCommandLine(COOKED_READ_DATA& cookedReadData)
{
    if (cookedReadData.IsEchoInput())
    {
        // Draw the command line
        cookedReadData.OriginalCursorPosition() = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();

        til::CoordType ScrollY = 0;
#pragma prefast(suppress : 28931, "Status is not unused. It's used in debug assertions.")
        auto Status = WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                       cookedReadData.BufferStartPtr(),
                                       cookedReadData.BufferStartPtr(),
                                       cookedReadData.BufferStartPtr(),
                                       &cookedReadData.BytesRead(),
                                       &cookedReadData.VisibleCharCount(),
                                       cookedReadData.OriginalCursorPosition().x,
                                       WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                       &ScrollY);
        FAIL_FAST_IF_NTSTATUS_FAILED(Status);

        cookedReadData.OriginalCursorPosition().y += ScrollY;

        // Move the cursor back to the right position
        auto CursorPosition = cookedReadData.OriginalCursorPosition();
        CursorPosition.x += RetrieveTotalNumberOfSpaces(cookedReadData.OriginalCursorPosition().x,
                                                        cookedReadData.BufferStartPtr(),
                                                        cookedReadData.InsertionPoint());
        if (CheckBisectStringW(cookedReadData.BufferStartPtr(),
                               cookedReadData.InsertionPoint(),
                               cookedReadData.ScreenInfo().GetBufferSize().Width() - cookedReadData.OriginalCursorPosition().x))
        {
            CursorPosition.x++;
        }
        AdjustCursorPosition(cookedReadData.ScreenInfo(), CursorPosition, TRUE, nullptr);
    }
}

// Routine Description:
// - This routine copies the commandline specified by Index into the cooked read buffer
void SetCurrentCommandLine(COOKED_READ_DATA& cookedReadData, _In_ CommandHistory::Index Index) // index, not command number
{
    DeleteCommandLine(cookedReadData, TRUE);
    FAIL_FAST_IF_FAILED(cookedReadData.History().RetrieveNth(Index,
                                                             cookedReadData.SpanWholeBuffer(),
                                                             cookedReadData.BytesRead()));
    FAIL_FAST_IF(!(cookedReadData.BufferStartPtr() == cookedReadData.BufferCurrentPtr()));
    if (cookedReadData.IsEchoInput())
    {
        til::CoordType ScrollY = 0;
        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      &cookedReadData.BytesRead(),
                                                      &cookedReadData.VisibleCharCount(),
                                                      cookedReadData.OriginalCursorPosition().x,
                                                      WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                      &ScrollY));
        cookedReadData.OriginalCursorPosition().y += ScrollY;
    }

    const auto CharsToWrite = cookedReadData.BytesRead() / sizeof(WCHAR);
    cookedReadData.InsertionPoint() = CharsToWrite;
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + CharsToWrite);
}

// Routine Description:
// - This routine handles the command list popup.  It puts up the popup, then calls ProcessCommandListInput to get and process input.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - STATUS_SUCCESS - read was fully completed (user hit return)
[[nodiscard]] NTSTATUS CommandLine::_startCommandListPopup(COOKED_READ_DATA& cookedReadData)
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
[[nodiscard]] NTSTATUS CommandLine::_startCopyFromCharPopup(COOKED_READ_DATA& cookedReadData)
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
[[nodiscard]] NTSTATUS CommandLine::_startCopyToCharPopup(COOKED_READ_DATA& cookedReadData)
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
[[nodiscard]] HRESULT CommandLine::StartCommandNumberPopup(COOKED_READ_DATA& cookedReadData)
{
    if (cookedReadData.HasHistory() &&
        cookedReadData.History().GetNumberOfCommands() &&
        cookedReadData.ScreenInfo().GetBufferSize().Width() >= Popup::MINIMUM_COMMAND_PROMPT_SIZE + 2)
    {
        try
        {
            auto& popup = *_popups.emplace_front(std::make_unique<CommandNumberPopup>(cookedReadData.ScreenInfo()));
            popup.Draw();

            // Save the original cursor position in case the user cancels out of the dialog
            cookedReadData.BeforeDialogCursorPosition() = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();

            // Move the cursor into the dialog so the user can type multiple characters for the command number
            const auto CursorPosition = popup.GetCursorPosition();
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
void CommandLine::_processHistoryCycling(COOKED_READ_DATA& cookedReadData,
                                         const CommandHistory::SearchDirection searchDirection)
{
    // for doskey compatibility, buffer isn't circular. don't do anything if attempting
    // to cycle history past the bounds of the history buffer
    if (!cookedReadData.HasHistory())
    {
        return;
    }
    else if (searchDirection == CommandHistory::SearchDirection::Previous && cookedReadData.History().AtFirstCommand())
    {
        return;
    }
    else if (searchDirection == CommandHistory::SearchDirection::Next && cookedReadData.History().AtLastCommand())
    {
        return;
    }

    DeleteCommandLine(cookedReadData, true);
    THROW_IF_FAILED(cookedReadData.History().Retrieve(searchDirection,
                                                      cookedReadData.SpanWholeBuffer(),
                                                      cookedReadData.BytesRead()));
    FAIL_FAST_IF(!(cookedReadData.BufferStartPtr() == cookedReadData.BufferCurrentPtr()));
    if (cookedReadData.IsEchoInput())
    {
        til::CoordType ScrollY = 0;
        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      &cookedReadData.BytesRead(),
                                                      &cookedReadData.VisibleCharCount(),
                                                      cookedReadData.OriginalCursorPosition().x,
                                                      WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                      &ScrollY));
        cookedReadData.OriginalCursorPosition().y += ScrollY;
    }
    const auto CharsToWrite = cookedReadData.BytesRead() / sizeof(WCHAR);
    cookedReadData.InsertionPoint() = CharsToWrite;
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + CharsToWrite);
}

// Routine Description:
// - Sets the text on the prompt to the oldest run command in the cookedReadData's history
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Note:
// - May throw exceptions
void CommandLine::_setPromptToOldestCommand(COOKED_READ_DATA& cookedReadData)
{
    if (cookedReadData.HasHistory() && cookedReadData.History().GetNumberOfCommands())
    {
        DeleteCommandLine(cookedReadData, true);
        const short commandNumber = 0;
        THROW_IF_FAILED(cookedReadData.History().RetrieveNth(commandNumber,
                                                             cookedReadData.SpanWholeBuffer(),
                                                             cookedReadData.BytesRead()));
        FAIL_FAST_IF(!(cookedReadData.BufferStartPtr() == cookedReadData.BufferCurrentPtr()));
        if (cookedReadData.IsEchoInput())
        {
            til::CoordType ScrollY = 0;
            FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                          cookedReadData.BufferStartPtr(),
                                                          cookedReadData.BufferCurrentPtr(),
                                                          cookedReadData.BufferCurrentPtr(),
                                                          &cookedReadData.BytesRead(),
                                                          &cookedReadData.VisibleCharCount(),
                                                          cookedReadData.OriginalCursorPosition().x,
                                                          WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                          &ScrollY));
            cookedReadData.OriginalCursorPosition().y += ScrollY;
        }
        auto CharsToWrite = cookedReadData.BytesRead() / sizeof(WCHAR);
        cookedReadData.InsertionPoint() = CharsToWrite;
        cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + CharsToWrite);
    }
}

// Routine Description:
// - Sets the text on the prompt the most recently run command in cookedReadData's history
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Note:
// - May throw exceptions
void CommandLine::_setPromptToNewestCommand(COOKED_READ_DATA& cookedReadData)
{
    DeleteCommandLine(cookedReadData, true);
    if (cookedReadData.HasHistory() && cookedReadData.History().GetNumberOfCommands())
    {
        const auto commandNumber = (SHORT)(cookedReadData.History().GetNumberOfCommands() - 1);
        THROW_IF_FAILED(cookedReadData.History().RetrieveNth(commandNumber,
                                                             cookedReadData.SpanWholeBuffer(),
                                                             cookedReadData.BytesRead()));
        FAIL_FAST_IF(!(cookedReadData.BufferStartPtr() == cookedReadData.BufferCurrentPtr()));
        if (cookedReadData.IsEchoInput())
        {
            til::CoordType ScrollY = 0;
            FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                          cookedReadData.BufferStartPtr(),
                                                          cookedReadData.BufferCurrentPtr(),
                                                          cookedReadData.BufferCurrentPtr(),
                                                          &cookedReadData.BytesRead(),
                                                          &cookedReadData.VisibleCharCount(),
                                                          cookedReadData.OriginalCursorPosition().x,
                                                          WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                          &ScrollY));
            cookedReadData.OriginalCursorPosition().y += ScrollY;
        }
        auto CharsToWrite = cookedReadData.BytesRead() / sizeof(WCHAR);
        cookedReadData.InsertionPoint() = CharsToWrite;
        cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + CharsToWrite);
    }
}

// Routine Description:
// - Deletes all prompt text to the right of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::DeletePromptAfterCursor(COOKED_READ_DATA& cookedReadData) noexcept
{
    DeleteCommandLine(cookedReadData, false);
    cookedReadData.BytesRead() = cookedReadData.InsertionPoint() * sizeof(WCHAR);
    if (cookedReadData.IsEchoInput())
    {
        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferStartPtr(),
                                                      &cookedReadData.BytesRead(),
                                                      &cookedReadData.VisibleCharCount(),
                                                      cookedReadData.OriginalCursorPosition().x,
                                                      WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                      nullptr));
    }
}

// Routine Description:
// - Deletes all user input on the prompt to the left of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_deletePromptBeforeCursor(COOKED_READ_DATA& cookedReadData) noexcept
{
    DeleteCommandLine(cookedReadData, false);
    cookedReadData.BytesRead() -= cookedReadData.InsertionPoint() * sizeof(WCHAR);
    cookedReadData.InsertionPoint() = 0;
    memmove(cookedReadData.BufferStartPtr(), cookedReadData.BufferCurrentPtr(), cookedReadData.BytesRead());
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr());
    if (cookedReadData.IsEchoInput())
    {
        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferStartPtr(),
                                                      &cookedReadData.BytesRead(),
                                                      &cookedReadData.VisibleCharCount(),
                                                      cookedReadData.OriginalCursorPosition().x,
                                                      WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                      nullptr));
    }
    return cookedReadData.OriginalCursorPosition();
}

// Routine Description:
// - Moves the cursor to the end of the prompt text
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_moveCursorToEndOfPrompt(COOKED_READ_DATA& cookedReadData) noexcept
{
    cookedReadData.InsertionPoint() = cookedReadData.BytesRead() / sizeof(WCHAR);
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + cookedReadData.InsertionPoint());
    til::point cursorPosition;
    cursorPosition.x = gsl::narrow<til::CoordType>(cookedReadData.OriginalCursorPosition().x + cookedReadData.VisibleCharCount());
    cursorPosition.y = cookedReadData.OriginalCursorPosition().y;

    const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
    if (CheckBisectProcessW(cookedReadData.ScreenInfo(),
                            cookedReadData.BufferStartPtr(),
                            cookedReadData.InsertionPoint(),
                            sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x,
                            cookedReadData.OriginalCursorPosition().x,
                            true))
    {
        cursorPosition.x++;
    }
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the start of the user input on the prompt
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_moveCursorToStartOfPrompt(COOKED_READ_DATA& cookedReadData) noexcept
{
    cookedReadData.InsertionPoint() = 0;
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr());
    return cookedReadData.OriginalCursorPosition();
}

// Routine Description:
// - Moves the cursor left by a word
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - New cursor position
til::point CommandLine::_moveCursorLeftByWord(COOKED_READ_DATA& cookedReadData) noexcept
{
    PWCHAR LastWord;
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    if (cookedReadData.BufferCurrentPtr() != cookedReadData.BufferStartPtr())
    {
        // A bit better word skipping.
        LastWord = cookedReadData.BufferCurrentPtr() - 1;
        if (LastWord != cookedReadData.BufferStartPtr())
        {
            if (*LastWord == L' ')
            {
                // Skip spaces, until the non-space character is found.
                while (--LastWord != cookedReadData.BufferStartPtr())
                {
                    FAIL_FAST_IF(!(LastWord > cookedReadData.BufferStartPtr()));
                    if (*LastWord != L' ')
                    {
                        break;
                    }
                }
            }
            if (LastWord != cookedReadData.BufferStartPtr())
            {
                if (IsWordDelim(*LastWord))
                {
                    // Skip WORD_DELIMs until space or non WORD_DELIM is found.
                    while (--LastWord != cookedReadData.BufferStartPtr())
                    {
                        FAIL_FAST_IF(!(LastWord > cookedReadData.BufferStartPtr()));
                        if (*LastWord == L' ' || !IsWordDelim(*LastWord))
                        {
                            break;
                        }
                    }
                }
                else
                {
                    // Skip the regular words
                    while (--LastWord != cookedReadData.BufferStartPtr())
                    {
                        FAIL_FAST_IF(!(LastWord > cookedReadData.BufferStartPtr()));
                        if (IsWordDelim(*LastWord))
                        {
                            break;
                        }
                    }
                }
            }
            FAIL_FAST_IF(!(LastWord >= cookedReadData.BufferStartPtr()));
            if (LastWord != cookedReadData.BufferStartPtr())
            {
                // LastWord is currently pointing to the last character
                // of the previous word, unless it backed up to the beginning
                // of the buffer.
                // Let's increment LastWord so that it points to the expected
                // insertion point.
                ++LastWord;
            }
            cookedReadData.SetBufferCurrentPtr(LastWord);
        }
        cookedReadData.InsertionPoint() = (cookedReadData.BufferCurrentPtr() - cookedReadData.BufferStartPtr());
        cursorPosition = cookedReadData.OriginalCursorPosition();
        cursorPosition.x = cursorPosition.x +
                           RetrieveTotalNumberOfSpaces(cookedReadData.OriginalCursorPosition().x,
                                                       cookedReadData.BufferStartPtr(),
                                                       cookedReadData.InsertionPoint());
        const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
        if (CheckBisectStringW(cookedReadData.BufferStartPtr(),
                               cookedReadData.InsertionPoint() + 1,
                               sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x))
        {
            cursorPosition.x++;
        }
    }
    return cursorPosition;
}

// Routine Description:
// - Moves cursor left by a glyph
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - New cursor position
til::point CommandLine::_moveCursorLeft(COOKED_READ_DATA& cookedReadData)
{
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    if (cookedReadData.BufferCurrentPtr() != cookedReadData.BufferStartPtr())
    {
        cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferCurrentPtr() - 1);
        cookedReadData.InsertionPoint()--;
        cursorPosition.x = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition().x;
        cursorPosition.y = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition().y;
        cursorPosition.x = cursorPosition.x -
                           RetrieveNumberOfSpaces(cookedReadData.OriginalCursorPosition().x,
                                                  cookedReadData.BufferStartPtr(),
                                                  cookedReadData.InsertionPoint());
        const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
        if (CheckBisectProcessW(cookedReadData.ScreenInfo(),
                                cookedReadData.BufferStartPtr(),
                                cookedReadData.InsertionPoint() + 2,
                                sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x,
                                cookedReadData.OriginalCursorPosition().x,
                                true))
        {
            if ((cursorPosition.x == -2) || (cursorPosition.x == -1))
            {
                cursorPosition.x--;
            }
        }
    }
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the right by a word
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_moveCursorRightByWord(COOKED_READ_DATA& cookedReadData) noexcept
{
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    if (cookedReadData.InsertionPoint() < (cookedReadData.BytesRead() / sizeof(WCHAR)))
    {
        auto NextWord = cookedReadData.BufferCurrentPtr();

        // A bit better word skipping.
        auto BufLast = cookedReadData.BufferStartPtr() + cookedReadData.BytesRead() / sizeof(WCHAR);

        FAIL_FAST_IF(!(NextWord < BufLast));
        if (*NextWord == L' ')
        {
            // If the current character is space, skip to the next non-space character.
            while (++NextWord < BufLast)
            {
                if (*NextWord != L' ')
                {
                    break;
                }
            }
        }
        else
        {
            // Skip the body part.
            auto fStartFromDelim = IsWordDelim(*NextWord);

            while (++NextWord < BufLast)
            {
                if (fStartFromDelim != IsWordDelim(*NextWord))
                {
                    break;
                }
            }

            // Skip the space block.
            for (; NextWord < BufLast; NextWord++)
            {
                if (*NextWord != L' ')
                {
                    break;
                }
            }
        }

        cookedReadData.SetBufferCurrentPtr(NextWord);
        cookedReadData.InsertionPoint() = (ULONG)(cookedReadData.BufferCurrentPtr() - cookedReadData.BufferStartPtr());
        cursorPosition = cookedReadData.OriginalCursorPosition();
        cursorPosition.x = cursorPosition.x +
                           RetrieveTotalNumberOfSpaces(cookedReadData.OriginalCursorPosition().x,
                                                       cookedReadData.BufferStartPtr(),
                                                       cookedReadData.InsertionPoint());
        const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
        if (CheckBisectStringW(cookedReadData.BufferStartPtr(),
                               cookedReadData.InsertionPoint() + 1,
                               sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x))
        {
            cursorPosition.x++;
        }
    }
    return cursorPosition;
}

// Routine Description:
// - Moves the cursor to the right by a glyph
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_moveCursorRight(COOKED_READ_DATA& cookedReadData) noexcept
{
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
    // If not at the end of the line, move cursor position right.
    if (cookedReadData.InsertionPoint() < (cookedReadData.BytesRead() / sizeof(WCHAR)))
    {
        cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
        cursorPosition.x = cursorPosition.x +
                           RetrieveNumberOfSpaces(cookedReadData.OriginalCursorPosition().x,
                                                  cookedReadData.BufferStartPtr(),
                                                  cookedReadData.InsertionPoint());
        if (CheckBisectProcessW(cookedReadData.ScreenInfo(),
                                cookedReadData.BufferStartPtr(),
                                cookedReadData.InsertionPoint() + 2,
                                sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x,
                                cookedReadData.OriginalCursorPosition().x,
                                true))
        {
            // Snap cursorPosition.x to sScreenBufferSizeX if it is at the edge of the screen
            if (cursorPosition.x == (sScreenBufferSizeX - 1))
                cursorPosition.x = sScreenBufferSizeX;
        }

        cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferCurrentPtr() + 1);
        cookedReadData.InsertionPoint()++;
    }
    // if at the end of the line, copy a character from the same position in the last command
    else if (cookedReadData.HasHistory())
    {
        size_t NumSpaces;
        const auto LastCommand = cookedReadData.History().GetLastCommand();
        if (!LastCommand.empty() && LastCommand.size() > cookedReadData.InsertionPoint())
        {
            *cookedReadData.BufferCurrentPtr() = LastCommand[cookedReadData.InsertionPoint()];
            cookedReadData.BytesRead() += sizeof(WCHAR);
            cookedReadData.InsertionPoint()++;
            if (cookedReadData.IsEchoInput())
            {
                til::CoordType ScrollY = 0;
                auto CharsToWrite = sizeof(WCHAR);
                FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                              cookedReadData.BufferStartPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              &CharsToWrite,
                                                              &NumSpaces,
                                                              cookedReadData.OriginalCursorPosition().x,
                                                              WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                              &ScrollY));
                cookedReadData.OriginalCursorPosition().y += ScrollY;
                cookedReadData.VisibleCharCount() += NumSpaces;
                // update reported cursor position
                if (ScrollY != 0)
                {
                    cursorPosition.x = 0;
                    cursorPosition.y += ScrollY;
                }
                else
                {
                    cursorPosition.x += 1;
                }
            }
            cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferCurrentPtr() + 1);
        }
    }
    return cursorPosition;
}

// Routine Description:
// - Place a ctrl-z in the current command line
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_insertCtrlZ(COOKED_READ_DATA& cookedReadData) noexcept
{
    size_t NumSpaces = 0;

    *cookedReadData.BufferCurrentPtr() = (WCHAR)0x1a; // ctrl-z
    cookedReadData.BytesRead() += sizeof(WCHAR);
    cookedReadData.InsertionPoint()++;
    if (cookedReadData.IsEchoInput())
    {
        til::CoordType ScrollY = 0;
        auto CharsToWrite = sizeof(WCHAR);
        FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                      cookedReadData.BufferStartPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      cookedReadData.BufferCurrentPtr(),
                                                      &CharsToWrite,
                                                      &NumSpaces,
                                                      cookedReadData.OriginalCursorPosition().x,
                                                      WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                      &ScrollY));
        cookedReadData.OriginalCursorPosition().y += ScrollY;
        cookedReadData.VisibleCharCount() += NumSpaces;
    }
    cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferCurrentPtr() + 1);
}

// Routine Description:
// - Empties the command history for cookedReadData
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_deleteCommandHistory(COOKED_READ_DATA& cookedReadData) noexcept
{
    if (cookedReadData.HasHistory())
    {
        cookedReadData.History().Empty();
        cookedReadData.History().Flags |= CommandHistory::CLE_ALLOCATED;
    }
}

// Routine Description:
// - Copy the remainder of the previous command to the current command.
// Arguments:
// - cookedReadData - The cooked read data to operate on
void CommandLine::_fillPromptWithPreviousCommandFragment(COOKED_READ_DATA& cookedReadData) noexcept
{
    if (cookedReadData.HasHistory())
    {
        size_t NumSpaces, cchCount;

        const auto LastCommand = cookedReadData.History().GetLastCommand();
        if (!LastCommand.empty() && LastCommand.size() > cookedReadData.InsertionPoint())
        {
            cchCount = LastCommand.size() - cookedReadData.InsertionPoint();
            const auto bufferSpan = cookedReadData.SpanAtPointer();
            std::copy_n(LastCommand.cbegin() + cookedReadData.InsertionPoint(), cchCount, bufferSpan.begin());
            cookedReadData.InsertionPoint() += cchCount;
            cchCount *= sizeof(WCHAR);
            cookedReadData.BytesRead() = std::max(LastCommand.size() * sizeof(wchar_t), cookedReadData.BytesRead());
            if (cookedReadData.IsEchoInput())
            {
                til::CoordType ScrollY = 0;
                FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                              cookedReadData.BufferStartPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              &cchCount,
                                                              &NumSpaces,
                                                              cookedReadData.OriginalCursorPosition().x,
                                                              WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                              &ScrollY));
                cookedReadData.OriginalCursorPosition().y += ScrollY;
                cookedReadData.VisibleCharCount() += NumSpaces;
            }
            cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferCurrentPtr() + cchCount / sizeof(WCHAR));
        }
    }
}

// Routine Description:
// - Cycles through the stored commands that start with the characters in the current command.
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::_cycleMatchingCommandHistoryToPrompt(COOKED_READ_DATA& cookedReadData)
{
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    if (cookedReadData.HasHistory())
    {
        CommandHistory::Index index;
        if (cookedReadData.History().FindMatchingCommand({ cookedReadData.BufferStartPtr(), cookedReadData.InsertionPoint() },
                                                         cookedReadData.History().LastDisplayed,
                                                         index,
                                                         CommandHistory::MatchOptions::None))
        {
            // save cursor position
            const auto CurrentPos = cookedReadData.InsertionPoint();

            DeleteCommandLine(cookedReadData, true);
            THROW_IF_FAILED(cookedReadData.History().RetrieveNth(index,
                                                                 cookedReadData.SpanWholeBuffer(),
                                                                 cookedReadData.BytesRead()));
            FAIL_FAST_IF(!(cookedReadData.BufferStartPtr() == cookedReadData.BufferCurrentPtr()));
            if (cookedReadData.IsEchoInput())
            {
                til::CoordType ScrollY = 0;
                FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                              cookedReadData.BufferStartPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              cookedReadData.BufferCurrentPtr(),
                                                              &cookedReadData.BytesRead(),
                                                              &cookedReadData.VisibleCharCount(),
                                                              cookedReadData.OriginalCursorPosition().x,
                                                              WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                              &ScrollY));
                cookedReadData.OriginalCursorPosition().y += ScrollY;
                cursorPosition.y += ScrollY;
            }

            // restore cursor position
            cookedReadData.SetBufferCurrentPtr(cookedReadData.BufferStartPtr() + CurrentPos);
            cookedReadData.InsertionPoint() = CurrentPos;
            FAIL_FAST_IF_NTSTATUS_FAILED(cookedReadData.ScreenInfo().SetCursorPosition(cursorPosition, true));
        }
    }
    return cursorPosition;
}

// Routine Description:
// - Deletes a glyph from the right side of the cursor
// Arguments:
// - cookedReadData - The cooked read data to operate on
// Return Value:
// - The new cursor position
til::point CommandLine::DeleteFromRightOfCursor(COOKED_READ_DATA& cookedReadData) noexcept
{
    // save cursor position
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();

    if (!cookedReadData.AtEol())
    {
        // Delete commandline.
        // clang-format off
#pragma prefast(suppress: __WARNING_BUFFER_OVERFLOW, "Not sure why prefast is getting confused here")
        // clang-format on
        DeleteCommandLine(cookedReadData, false);

        // Delete char.
        cookedReadData.BytesRead() -= sizeof(WCHAR);
        memmove(cookedReadData.BufferCurrentPtr(),
                cookedReadData.BufferCurrentPtr() + 1,
                cookedReadData.BytesRead() - (cookedReadData.InsertionPoint() * sizeof(WCHAR)));

        {
            auto buf = (PWCHAR)((PBYTE)cookedReadData.BufferStartPtr() + cookedReadData.BytesRead());
            *buf = (WCHAR)' ';
        }

        // Write commandline.
        if (cookedReadData.IsEchoInput())
        {
            FAIL_FAST_IF_NTSTATUS_FAILED(WriteCharsLegacy(cookedReadData.ScreenInfo(),
                                                          cookedReadData.BufferStartPtr(),
                                                          cookedReadData.BufferStartPtr(),
                                                          cookedReadData.BufferStartPtr(),
                                                          &cookedReadData.BytesRead(),
                                                          &cookedReadData.VisibleCharCount(),
                                                          cookedReadData.OriginalCursorPosition().x,
                                                          WC_INTERACTIVE | WC_KEEP_CURSOR_VISIBLE,
                                                          nullptr));
        }

        // restore cursor position
        const auto sScreenBufferSizeX = cookedReadData.ScreenInfo().GetBufferSize().Width();
        if (CheckBisectProcessW(cookedReadData.ScreenInfo(),
                                cookedReadData.BufferStartPtr(),
                                cookedReadData.InsertionPoint() + 1,
                                sScreenBufferSizeX - cookedReadData.OriginalCursorPosition().x,
                                cookedReadData.OriginalCursorPosition().x,
                                true))
        {
            cursorPosition.x++;
        }
    }
    return cursorPosition;
}

// TODO: [MSFT:4586207] Clean up this mess -- needs helpers. http://osgvsowi/4586207
// Routine Description:
// - This routine process command line editing keys.
// Return Value:
// - CONSOLE_STATUS_WAIT - CommandListPopup ran out of input
// - CONSOLE_STATUS_READ_COMPLETE - user hit <enter> in CommandListPopup
// - STATUS_SUCCESS - everything's cool
[[nodiscard]] NTSTATUS CommandLine::ProcessCommandLine(COOKED_READ_DATA& cookedReadData,
                                                       _In_ WCHAR wch,
                                                       const DWORD dwKeyState)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto cursorPosition = cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().GetPosition();
    NTSTATUS Status;

    const auto altPressed = WI_IsAnyFlagSet(dwKeyState, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
    const auto ctrlPressed = WI_IsAnyFlagSet(dwKeyState, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
    auto UpdateCursorPosition = false;
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
        }
        else if (altPressed)
        {
            _deleteCommandHistory(cookedReadData);
        }
        break;

    case VK_F8:
        try
        {
            cursorPosition = _cycleMatchingCommandHistoryToPrompt(cookedReadData);
            UpdateCursorPosition = true;
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
        cursorPosition = DeleteFromRightOfCursor(cookedReadData);
        UpdateCursorPosition = true;
        break;
    default:
        FAIL_FAST_HR(E_NOTIMPL);
        break;
    }

    if (UpdateCursorPosition && cookedReadData.IsEchoInput())
    {
        AdjustCursorPosition(cookedReadData.ScreenInfo(), cursorPosition, true, nullptr);
    }

    return STATUS_SUCCESS;
}
