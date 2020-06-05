// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#ifndef CON_USERPRIVAPI_INDIRECT
#include <user32p.h>
#endif

#include "ConsoleControl.hpp"

#include "..\..\interactivity\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

#pragma region IConsoleControl Members

[[nodiscard]] NTSTATUS ConsoleControl::NotifyConsoleApplication(_In_ DWORD dwProcessId)
{
    CONSOLE_PROCESS_INFO cpi;
    cpi.dwProcessID = dwProcessId;
    cpi.dwFlags = CPI_NEWPROCESSWINDOW;

    return Control(ControlType::ConsoleNotifyConsoleApplication,
                   &cpi,
                   sizeof(CONSOLE_PROCESS_INFO));
}

[[nodiscard]] NTSTATUS ConsoleControl::SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground)
{
    CONSOLESETFOREGROUND Flags;
    Flags.hProcess = hProcess;
    Flags.bForeground = fForeground;

    return Control(ControlType::ConsoleSetForeground,
                   &Flags,
                   sizeof(Flags));
}

[[nodiscard]] NTSTATUS ConsoleControl::EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    auto pConsoleWindow = ServiceLocator::LocateConsoleWindow();

    CONSOLEENDTASK ConsoleEndTaskParams;
    ConsoleEndTaskParams.ProcessId = hProcessId;
    ConsoleEndTaskParams.ConsoleEventCode = dwEventType;
    ConsoleEndTaskParams.ConsoleFlags = ulCtrlFlags;
    ConsoleEndTaskParams.hwnd = pConsoleWindow == nullptr ? nullptr : pConsoleWindow->GetWindowHandle();

    return Control(ControlType::ConsoleEndTask,
                   &ConsoleEndTaskParams,
                   sizeof(ConsoleEndTaskParams));
}

#pragma endregion

#pragma region Public Methods

[[nodiscard]] NTSTATUS ConsoleControl::Control(_In_ ControlType ConsoleCommand,
                                               _In_reads_bytes_(ConsoleInformationLength) PVOID ConsoleInformation,
                                               _In_ DWORD ConsoleInformationLength)
{
#ifdef CON_USERPRIVAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef NTSTATUS(WINAPI * PfnConsoleControl)(ControlType Command, PVOID Information, DWORD Length);

        static PfnConsoleControl pfn = (PfnConsoleControl)GetProcAddress(_hUser32, "ConsoleControl");

        if (pfn != nullptr)
        {
            return pfn(ConsoleCommand, ConsoleInformation, ConsoleInformationLength);
        }
    }

    return STATUS_UNSUCCESSFUL;
#else
    return ConsoleControl(ConsoleCommand, ConsoleInformation, ConsoleInformationLength);
#endif
}

BOOL ConsoleControl::EnterReaderModeHelper(_In_ HWND hwnd)
{
#ifdef CON_USERPRIVAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef BOOL(WINAPI * PfnEnterReaderModeHelper)(HWND hwnd);

        static PfnEnterReaderModeHelper pfn = (PfnEnterReaderModeHelper)GetProcAddress(_hUser32, "EnterReaderModeHelper");

        if (pfn != nullptr)
        {
            return pfn(hwnd);
        }
    }

    return FALSE;
#else
    return EnterReaderModeHelper(hwnd);
#endif
}

BOOL ConsoleControl::TranslateMessageEx(const MSG* pmsg,
                                        _In_ UINT flags)
{
#ifdef CON_USERPRIVAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef BOOL(WINAPI * PfnTranslateMessageEx)(const MSG* pmsg, UINT flags);

        static PfnTranslateMessageEx pfn = (PfnTranslateMessageEx)GetProcAddress(_hUser32, "TranslateMessageEx");

        if (pfn != nullptr)
        {
            return pfn(pmsg, flags);
        }
    }

    return FALSE;
#else
    return TranslateMessageEx(pmsg, flags);
#endif
}

#pragma endregion

#ifdef CON_USERPRIVAPI_INDIRECT
ConsoleControl::ConsoleControl()
{
    _hUser32 = LoadLibraryW(L"user32.dll");
}

ConsoleControl::~ConsoleControl()
{
    if (_hUser32 != nullptr)
    {
        FreeLibrary(_hUser32);
        _hUser32 = nullptr;
    }
}
#endif
