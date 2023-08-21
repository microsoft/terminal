// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <intsafe.h>

// MidiAudio
#include <mmeapi.h>
#include <dsound.h>

#include "misc.h"
#include "output.h"
#include "srvinit.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/convert.hpp"

using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::VtIo;

bool CONSOLE_INFORMATION::IsConsoleLocked() const noexcept
{
    return _lock.is_locked();
}

#pragma prefast(suppress : 26135, "Adding lock annotation spills into entire project. Future work.")
void CONSOLE_INFORMATION::LockConsole() noexcept
{
    _lock.lock();
}

#pragma prefast(suppress : 26135, "Adding lock annotation spills into entire project. Future work.")
void CONSOLE_INFORMATION::UnlockConsole() noexcept
{
    _lock.unlock();
}

ULONG CONSOLE_INFORMATION::GetCSRecursionCount() const noexcept
{
    return _lock.recursion_depth();
}

// Routine Description:
// - This routine allocates and initialized a console and its associated
//   data - input buffer and screen buffer.
// - NOTE: Will read global ServiceLocator::LocateGlobals().getConsoleInformation expecting Settings to already be filled.
// Arguments:
// - title - Window Title to display
// Return Value:
// - STATUS_SUCCESS if successful.
[[nodiscard]] NTSTATUS CONSOLE_INFORMATION::AllocateConsole(const std::wstring_view title)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // Synchronize flags
    WI_SetFlagIf(gci.Flags, CONSOLE_AUTO_POSITION, !!gci.GetAutoPosition());
    WI_SetFlagIf(gci.Flags, CONSOLE_QUICK_EDIT_MODE, !!gci.GetQuickEdit());
    WI_SetFlagIf(gci.Flags, CONSOLE_HISTORY_NODUP, !!gci.GetHistoryNoDup());

    const auto pSelection = &Selection::Instance();
    pSelection->SetLineSelection(!!gci.GetLineSelection());

    SetConsoleCPInfo(TRUE);
    SetConsoleCPInfo(FALSE);

    // Initialize input buffer.
    try
    {
        gci.pInputBuffer = new InputBuffer();
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    try
    {
        gci.SetTitle(title);

        // TranslateConsoleTitle must have a null terminated string.
        // This should only happen once on startup so the copy shouldn't be costly
        // but could be eliminated by rewriting TranslateConsoleTitle.
        const std::wstring nullTerminatedTitle{ gci.GetTitle() };
        gci.SetOriginalTitle(std::wstring(TranslateConsoleTitle(nullTerminatedTitle.c_str(), TRUE, FALSE)));
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    auto Status = DoCreateScreenBuffer();
    if (FAILED_NTSTATUS(Status))
    {
        goto ErrorExit2;
    }

    gci.SetActiveOutputBuffer(*gci.ScreenBuffers);

    gci.GetActiveOutputBuffer().ScrollScale = gci.GetScrollScale();

    gci.ConsoleIme.RefreshAreaAttributes();

    if (SUCCEEDED_NTSTATUS(Status))
    {
        return STATUS_SUCCESS;
    }

    RIPMSG1(RIP_WARNING, "Console init failed with status 0x%x", Status);

    delete gci.ScreenBuffers;
    gci.ScreenBuffers = nullptr;

ErrorExit2:
    delete gci.pInputBuffer;

    return Status;
}

VtIo* CONSOLE_INFORMATION::GetVtIo()
{
    return &_vtIo;
}

bool CONSOLE_INFORMATION::IsInVtIoMode() const
{
    return _vtIo.IsUsingVt();
}

bool CONSOLE_INFORMATION::HasPendingCookedRead() const noexcept
{
    return _cookedReadData != nullptr;
}

const COOKED_READ_DATA& CONSOLE_INFORMATION::CookedReadData() const noexcept
{
    return *_cookedReadData;
}

COOKED_READ_DATA& CONSOLE_INFORMATION::CookedReadData() noexcept
{
    return *_cookedReadData;
}

void CONSOLE_INFORMATION::SetCookedReadData(COOKED_READ_DATA* readData) noexcept
{
    _cookedReadData = readData;
}

bool CONSOLE_INFORMATION::GetBracketedPasteMode() const noexcept
{
    return _bracketedPasteMode;
}

void CONSOLE_INFORMATION::SetBracketedPasteMode(const bool enabled) noexcept
{
    _bracketedPasteMode = enabled;
}

// Method Description:
// - Return the active screen buffer of the console.
// Arguments:
// - <none>
// Return Value:
// - the active screen buffer of the console.
SCREEN_INFORMATION& CONSOLE_INFORMATION::GetActiveOutputBuffer()
{
    return *pCurrentScreenBuffer;
}

const SCREEN_INFORMATION& CONSOLE_INFORMATION::GetActiveOutputBuffer() const
{
    return *pCurrentScreenBuffer;
}

void CONSOLE_INFORMATION::SetActiveOutputBuffer(SCREEN_INFORMATION& screenBuffer)
{
    if (pCurrentScreenBuffer)
    {
        pCurrentScreenBuffer->GetTextBuffer().SetAsActiveBuffer(false);
    }
    pCurrentScreenBuffer = &screenBuffer;
    pCurrentScreenBuffer->GetTextBuffer().SetAsActiveBuffer(true);
}

bool CONSOLE_INFORMATION::HasActiveOutputBuffer() const
{
    return (pCurrentScreenBuffer != nullptr);
}

// Method Description:
// - Return the active input buffer of the console.
// Arguments:
// - <none>
// Return Value:
// - the active input buffer of the console.
InputBuffer* const CONSOLE_INFORMATION::GetActiveInputBuffer() const
{
    return pInputBuffer;
}

// Method Description:
// - Set the console's title, and trigger a renderer update of the title.
//      This does not include the title prefix, such as "Mark", "Select", or "Scroll"
// Arguments:
// - newTitle: The new value to use for the title
// Return Value:
// - <none>
void CONSOLE_INFORMATION::SetTitle(const std::wstring_view newTitle)
{
    _Title = std::wstring{ newTitle.begin(), newTitle.end() };

    // Sanitize the input if we're in pty mode. No control chars - this string
    //      will get emitted back to the TTY in a VT sequence, and we don't want
    //      to embed control characters in that string. Note that we can't use
    //      IsInVtIoMode for this test, because the VT I/O thread won't have
    //      been created when the title is first set during startup.
    if (ServiceLocator::LocateGlobals().launchArgs.InConptyMode())
    {
        _Title.erase(std::remove_if(_Title.begin(), _Title.end(), [](auto ch) {
                         return ch < UNICODE_SPACE || (ch > UNICODE_DEL && ch < UNICODE_NBSP);
                     }),
                     _Title.end());
    }

    _TitleAndPrefix = _Prefix + _Title;

    auto* const pRender = ServiceLocator::LocateGlobals().pRender;
    if (pRender)
    {
        pRender->TriggerTitleChange();
    }
}

// Method Description:
// - Set the console title's prefix, and trigger a renderer update of the title.
//      This is the part of the title such as "Mark", "Select", or "Scroll"
// Arguments:
// - newTitlePrefix: The new value to use for the title prefix
// Return Value:
// - <none>
void CONSOLE_INFORMATION::SetTitlePrefix(const std::wstring_view newTitlePrefix)
{
    _Prefix = newTitlePrefix;
    _TitleAndPrefix = _Prefix + _Title;

    auto* const pRender = ServiceLocator::LocateGlobals().pRender;
    if (pRender)
    {
        pRender->TriggerTitleChange();
    }
}

// Method Description:
// - Set the value of the console's original title. This is the title the
//      console launched with.
// Arguments:
// - originalTitle: The new value to use for the console's original title
// Return Value:
// - <none>
void CONSOLE_INFORMATION::SetOriginalTitle(const std::wstring_view originalTitle)
{
    _OriginalTitle = originalTitle;
}

// Method Description:
// - Set the value of the console's link title. If the console was launched
///     from a shortcut, this value will not be the empty string.
// Arguments:
// - linkTitle: The new value to use for the console's link title
// Return Value:
// - <none>
void CONSOLE_INFORMATION::SetLinkTitle(const std::wstring_view linkTitle)
{
    _LinkTitle = linkTitle;
}

// Method Description:
// - return a reference to the console's title.
// Arguments:
// - <none>
// Return Value:
// - the console's title.
const std::wstring_view CONSOLE_INFORMATION::GetTitle() const noexcept
{
    return _Title;
}

// Method Description:
// - Return a new wstring representing the actual display value of the title.
//      This is the Prefix+Title.
// Arguments:
// - <none>
// Return Value:
// - the combined prefix and title.
const std::wstring_view CONSOLE_INFORMATION::GetTitleAndPrefix() const
{
    return _TitleAndPrefix;
}

// Method Description:
// - return a reference to the console's original title.
// Arguments:
// - <none>
// Return Value:
// - the console's original title.
const std::wstring_view CONSOLE_INFORMATION::GetOriginalTitle() const noexcept
{
    return _OriginalTitle;
}

// Method Description:
// - return a reference to the console's link title.
// Arguments:
// - <none>
// Return Value:
// - the console's link title.
const std::wstring_view CONSOLE_INFORMATION::GetLinkTitle() const noexcept
{
    return _LinkTitle;
}

// Method Description:
// - return a reference to the console's cursor blinker.
// Arguments:
// - <none>
// Return Value:
// - a reference to the console's cursor blinker.
Microsoft::Console::CursorBlinker& CONSOLE_INFORMATION::GetCursorBlinker() noexcept
{
    return _blinker;
}

// Method Description:
// - Returns the MIDI audio instance.
// Arguments:
// - <none>
// Return Value:
// - a reference to the MidiAudio instance.
MidiAudio& CONSOLE_INFORMATION::GetMidiAudio()
{
    return _midiAudio;
}

// Method Description:
// - Generates a CHAR_INFO for this output cell, using the TextAttribute
//      GetLegacyAttributes method to generate the legacy style attributes.
// Arguments:
// - cell: The cell to get the CHAR_INFO from
// Return Value:
// - a CHAR_INFO containing legacy information about the cell
CHAR_INFO CONSOLE_INFORMATION::AsCharInfo(const OutputCellView& cell) const noexcept
{
    CHAR_INFO ci{ 0 };
    ci.Char.UnicodeChar = Utf16ToUcs2(cell.Chars());

    // If the current text attributes aren't legacy attributes, then
    //    use gci to look up the correct legacy attributes to use
    //    (for mapping RGB values to the nearest table value)
    const auto& attr = cell.TextAttr();
    ci.Attributes = attr.GetLegacyAttributes();
    ci.Attributes |= GeneratePublicApiAttributeFormat(cell.DbcsAttr());
    return ci;
}
