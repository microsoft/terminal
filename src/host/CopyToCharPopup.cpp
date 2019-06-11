// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "CopyToCharPopup.hpp"

#include "stream.h"
#include "_stream.h"
#include "resource.h"

static constexpr size_t COPY_TO_CHAR_PROMPT_LENGTH = 26;

CopyToCharPopup::CopyToCharPopup(SCREEN_INFORMATION& screenInfo) :
    Popup(screenInfo, { COPY_TO_CHAR_PROMPT_LENGTH + 2, 1 })
{
}

// Routine Description:
// - copies text from the previous command into the current prompt line, up to but not including the first
// instance of wch after the current cookedReadData's cursor position. if wch is not found, nothing is copied.
// Arguments:
// - cookedReadData - the read data to operate on
// - LastCommand - the most recent command run
// - wch - the wchar to copy up to
void CopyToCharPopup::_copyToChar(COOKED_READ_DATA& cookedReadData, const std::wstring_view LastCommand, const wchar_t wch)
{
    // make sure that there it is possible to copy any found text over
    if (cookedReadData.InsertionPoint() >= LastCommand.size())
    {
        return;
    }

    const auto searchStart = std::next(LastCommand.cbegin(), cookedReadData.InsertionPoint() + 1);
    auto location = std::find(searchStart, LastCommand.cend(), wch);

    // didn't find wch so copy nothing
    if (location == LastCommand.cend())
    {
        return;
    }

    const auto startIt = std::next(LastCommand.cbegin(), cookedReadData.InsertionPoint());
    const auto endIt = location;

    cookedReadData.Write({ &*startIt, gsl::narrow<size_t>(std::distance(startIt, endIt)) });
}

// Routine Description:
// - This routine handles the delete char popup.  It returns when we're out of input or the user has entered a char.
// Return Value:
// - CONSOLE_STATUS_WAIT - we ran out of input, so a wait block was created
// - CONSOLE_STATUS_READ_COMPLETE - user hit return
[[nodiscard]] NTSTATUS CopyToCharPopup::Process(COOKED_READ_DATA& cookedReadData) noexcept
{
    wchar_t wch = UNICODE_NULL;
    bool popupKey = false;
    DWORD modifiers = 0;
    NTSTATUS Status = _getUserInput(cookedReadData, popupKey, modifiers, wch);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    CommandLine::Instance().EndCurrentPopup();

    if (popupKey && wch == VK_ESCAPE)
    {
        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }

    // copy up to specified char
    const auto lastCommand = cookedReadData.History().GetLastCommand();
    if (!lastCommand.empty())
    {
        _copyToChar(cookedReadData, lastCommand, wch);
    }

    return CONSOLE_STATUS_WAIT_NO_BLOCK;
}

void CopyToCharPopup::_DrawContent()
{
    _DrawPrompt(ID_CONSOLE_MSGCMDLINEF2);
}
