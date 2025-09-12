// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#ifndef CON_USERPRIVAPI_INDIRECT
#include <user32p.h>
#endif

#include "ConsoleControl.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

#ifdef CON_USERPRIVAPI_INDIRECT
ConsoleControl::ConsoleControl()
{
    // NOTE: GetModuleHandleW is rather expensive, but GetProcAddress is quite cheap.
    const auto user32 = GetModuleHandleW(L"user32.dll");
    _consoleControl = reinterpret_cast<PfnConsoleControl>(GetProcAddress(user32, "ConsoleControl"));
    _enterReaderModeHelper = reinterpret_cast<PfnEnterReaderModeHelper>(GetProcAddress(user32, "EnterReaderModeHelper"));
    _translateMessageEx = reinterpret_cast<PfnTranslateMessageEx>(GetProcAddress(user32, "TranslateMessageEx"));
}
#endif

void ConsoleControl::Control(ControlType command, PVOID ptr, DWORD len) noexcept
{
#ifdef CON_USERPRIVAPI_INDIRECT
    if (_consoleControl)
    {
        LOG_IF_FAILED(_consoleControl(command, ptr, len));
    }
#else
    LOG_IF_FAILED(::ConsoleControl(command, ptr, len));
#endif
}

void ConsoleControl::NotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) noexcept
{
    ::NotifyWinEvent(event, hwnd, idObject, idChild);
}

void ConsoleControl::NotifyConsoleApplication(_In_ DWORD dwProcessId) noexcept
{
    CONSOLE_PROCESS_INFO cpi;
    cpi.dwProcessID = dwProcessId;
    cpi.dwFlags = CPI_NEWPROCESSWINDOW;
    Control(ControlType::ConsoleNotifyConsoleApplication, &cpi, sizeof(CONSOLE_PROCESS_INFO));
}

void ConsoleControl::SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground) noexcept
{
    CONSOLESETFOREGROUND Flags;
    Flags.hProcess = hProcess;
    Flags.bForeground = fForeground;
    Control(ControlType::ConsoleSetForeground, &Flags, sizeof(Flags));
}

void ConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) noexcept
{
    auto pConsoleWindow = ServiceLocator::LocateConsoleWindow();

    CONSOLEENDTASK ConsoleEndTaskParams;
    ConsoleEndTaskParams.ProcessId = ULongToHandle(dwProcessId); // This is actually a PID, even though the struct expects a HANDLE.
    ConsoleEndTaskParams.ConsoleEventCode = dwEventType;
    ConsoleEndTaskParams.ConsoleFlags = ulCtrlFlags;
    ConsoleEndTaskParams.hwnd = pConsoleWindow == nullptr ? nullptr : pConsoleWindow->GetWindowHandle();
    Control(ControlType::ConsoleEndTask, &ConsoleEndTaskParams, sizeof(ConsoleEndTaskParams));
}

void ConsoleControl::SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept
{
    CONSOLEWINDOWOWNER ConsoleOwner;
    ConsoleOwner.hwnd = hwnd;
    ConsoleOwner.ProcessId = processId;
    ConsoleOwner.ThreadId = threadId;
    Control(ControlType::ConsoleSetWindowOwner, &ConsoleOwner, sizeof(ConsoleOwner));
}

BOOL ConsoleControl::EnterReaderModeHelper(_In_ HWND hwnd)
{
#ifdef CON_USERPRIVAPI_INDIRECT
    return _enterReaderModeHelper ? _enterReaderModeHelper(hwnd) : FALSE;
#else
    return ::EnterReaderModeHelper(hwnd);
#endif
}

BOOL ConsoleControl::TranslateMessageEx(const MSG* pmsg, _In_ UINT flags)
{
#ifdef CON_USERPRIVAPI_INDIRECT
    return _translateMessageEx ? _translateMessageEx(pmsg, flags) : FALSE;
#else
    return ::TranslateMessageEx(pmsg, flags);
#endif
}
