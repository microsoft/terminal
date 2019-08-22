// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <intsafe.h>

#include "misc.h"
#include "output.h"
#include "srvinit.h"

#include "..\interactivity\inc\ServiceLocator.hpp"
#include "..\types\inc\convert.hpp"

using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::VtIo;

CONSOLE_INFORMATION::CONSOLE_INFORMATION() :
    // ProcessHandleList initializes itself
    pInputBuffer(nullptr),
    pCurrentScreenBuffer(nullptr),
    ScreenBuffers(nullptr),
    OutputQueue(),
    // ExeAliasList initialized below
    _OriginalTitle(),
    _Title(),
    _LinkTitle(),
    Flags(0),
    PopupCount(0),
    CP(0),
    OutputCP(0),
    CtrlFlags(0),
    LimitingProcessId(0),
    // ColorTable initialized below
    // CPInfo initialized below
    // OutputCPInfo initialized below
    _cookedReadData(nullptr),
    ConsoleIme{},
    terminalMouseInput(HandleTerminalKeyEventCallback),
    _vtIo(),
    _blinker{},
    renderData{}
{
    ZeroMemory((void*)&CPInfo, sizeof(CPInfo));
    ZeroMemory((void*)&OutputCPInfo, sizeof(OutputCPInfo));
    InitializeCriticalSection(&_csConsoleLock);
}

CONSOLE_INFORMATION::~CONSOLE_INFORMATION()
{
    DeleteCriticalSection(&_csConsoleLock);
}

bool CONSOLE_INFORMATION::IsConsoleLocked() const
{
    // The critical section structure's OwningThread field contains the ThreadId despite having the HANDLE type.
    // This requires us to hard cast the ID to compare.
    return _csConsoleLock.OwningThread == (HANDLE)GetCurrentThreadId();
}

#pragma prefast(suppress : 26135, "Adding lock annotation spills into entire project. Future work.")
void CONSOLE_INFORMATION::LockConsole()
{
    EnterCriticalSection(&_csConsoleLock);
}

#pragma prefast(suppress : 26135, "Adding lock annotation spills into entire project. Future work.")
bool CONSOLE_INFORMATION::TryLockConsole()
{
    return !!TryEnterCriticalSection(&_csConsoleLock);
}

#pragma prefast(suppress : 26135, "Adding lock annotation spills into entire project. Future work.")
void CONSOLE_INFORMATION::UnlockConsole()
{
    LeaveCriticalSection(&_csConsoleLock);
}

ULONG CONSOLE_INFORMATION::GetCSRecursionCount()
{
    return _csConsoleLock.RecursionCount;
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
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // Synchronize flags
    WI_SetFlagIf(gci.Flags, CONSOLE_AUTO_POSITION, !!gci.GetAutoPosition());
    WI_SetFlagIf(gci.Flags, CONSOLE_QUICK_EDIT_MODE, !!gci.GetQuickEdit());
    WI_SetFlagIf(gci.Flags, CONSOLE_HISTORY_NODUP, !!gci.GetHistoryNoDup());

    Selection* const pSelection = &Selection::Instance();
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
        gci.SetOriginalTitle(std::wstring(TranslateConsoleTitle(gci.GetTitle().c_str(), TRUE, FALSE)));
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    NTSTATUS Status = DoCreateScreenBuffer();
    if (!NT_SUCCESS(Status))
    {
        goto ErrorExit2;
    }

    gci.pCurrentScreenBuffer = gci.ScreenBuffers;

    gci.GetActiveOutputBuffer().ScrollScale = gci.GetScrollScale();

    gci.ConsoleIme.RefreshAreaAttributes();

    if (NT_SUCCESS(Status))
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

// Routine Description:
// - Handler for inserting key sequences into the buffer when the terminal emulation layer
//   has determined a key can be converted appropriately into a sequence of inputs
// Arguments:
// - events - the input events to write to the input buffer
// Return Value:
// - <none>
void CONSOLE_INFORMATION::HandleTerminalKeyEventCallback(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events)
{
    ServiceLocator::LocateGlobals().getConsoleInformation().pInputBuffer->Write(events);
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
// - Return the default foreground color of the console. If the settings are
//      configured to have a default foreground color (separate from the color
//      table), this will return that value. Otherwise it will return the value
//      from the colortable corresponding to our default attributes.
// Arguments:
// - <none>
// Return Value:
// - the default foreground color of the console.
COLORREF CONSOLE_INFORMATION::GetDefaultForeground() const noexcept
{
    return Settings::CalculateDefaultForeground();
}

// Method Description:
// - Return the default background color of the console. If the settings are
//      configured to have a default background color (separate from the color
//      table), this will return that value. Otherwise it will return the value
//      from the colortable corresponding to our default attributes.
// Arguments:
// - <none>
// Return Value:
// - the default background color of the console.
COLORREF CONSOLE_INFORMATION::GetDefaultBackground() const noexcept
{
    return Settings::CalculateDefaultBackground();
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
void CONSOLE_INFORMATION::SetTitlePrefix(const std::wstring& newTitlePrefix)
{
    _TitlePrefix = newTitlePrefix;

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
void CONSOLE_INFORMATION::SetOriginalTitle(const std::wstring& originalTitle)
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
void CONSOLE_INFORMATION::SetLinkTitle(const std::wstring& linkTitle)
{
    _LinkTitle = linkTitle;
}

// Method Description:
// - return a reference to the console's title.
// Arguments:
// - <none>
// Return Value:
// - a reference to the console's title.
const std::wstring& CONSOLE_INFORMATION::GetTitle() const noexcept
{
    return _Title;
}

// Method Description:
// - Return a new wstring representing the actual display value of the title.
//      This is the Prefix+Title.
// Arguments:
// - <none>
// Return Value:
// - a new wstring containing the combined prefix and title.
const std::wstring CONSOLE_INFORMATION::GetTitleAndPrefix() const
{
    return _TitlePrefix + _Title;
}

// Method Description:
// - return a reference to the console's original title.
// Arguments:
// - <none>
// Return Value:
// - a reference to the console's original title.
const std::wstring& CONSOLE_INFORMATION::GetOriginalTitle() const noexcept
{
    return _OriginalTitle;
}

// Method Description:
// - return a reference to the console's link title.
// Arguments:
// - <none>
// Return Value:
// - a reference to the console's link title.
const std::wstring& CONSOLE_INFORMATION::GetLinkTitle() const noexcept
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
// - Generates a CHAR_INFO for this output cell, using our
//      GenerateLegacyAttributes method to generate the legacy style attributes.
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
    ci.Attributes = GenerateLegacyAttributes(attr);
    ;
    ci.Attributes |= cell.DbcsAttr().GeneratePublicApiAttributeFormat();
    return ci;
}
