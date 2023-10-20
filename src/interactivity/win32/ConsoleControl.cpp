// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#ifndef CON_USERPRIVAPI_INDIRECT
#include <user32p.h>
#endif

#include "ConsoleControl.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

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

[[nodiscard]] NTSTATUS ConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    auto pConsoleWindow = ServiceLocator::LocateConsoleWindow();

    CONSOLEENDTASK ConsoleEndTaskParams;
    ConsoleEndTaskParams.ProcessId = ULongToHandle(dwProcessId); // This is actually a PID, even though the struct expects a HANDLE.
    ConsoleEndTaskParams.ConsoleEventCode = dwEventType;
    ConsoleEndTaskParams.ConsoleFlags = ulCtrlFlags;
    ConsoleEndTaskParams.hwnd = pConsoleWindow == nullptr ? nullptr : pConsoleWindow->GetWindowHandle();

    return Control(ControlType::ConsoleEndTask,
                   &ConsoleEndTaskParams,
                   sizeof(ConsoleEndTaskParams));
}

[[nodiscard]] NTSTATUS ConsoleControl::SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept
{
    CONSOLEWINDOWOWNER ConsoleOwner;
    ConsoleOwner.hwnd = hwnd;
    ConsoleOwner.ProcessId = processId;
    ConsoleOwner.ThreadId = threadId;

    return Control(ConsoleControl::ControlType::ConsoleSetWindowOwner,
                   &ConsoleOwner,
                   sizeof(ConsoleOwner));
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

        static auto pfn = (PfnConsoleControl)GetProcAddress(_hUser32, "ConsoleControl");

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

        static auto pfn = (PfnEnterReaderModeHelper)GetProcAddress(_hUser32, "EnterReaderModeHelper");

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

        static auto pfn = (PfnTranslateMessageEx)GetProcAddress(_hUser32, "TranslateMessageEx");

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

DWORD GetRootProcessId() {
    // Implement your logic to get the root process ID here
    // Replace this with the actual logic to determine the root PID
    return 12345; // Example PID
}

HWND GetPseudoConsoleWindowHandle() {
    // Implement your logic to get the pseudoconsole window handle here
    // Replace this with the actual logic to obtain the window handle
    return GetConsoleWindow(); // Example: use the console window handle
}

int main() {
    // Initialize your application

    // Get the root process ID
    DWORD rootProcessId = GetRootProcessId(); // Replace with your method to get the root PID

    // Get the pseudoconsole window handle
    HWND pseudoConsoleWindowHandle = GetPseudoConsoleWindowHandle(); // Replace with your method to get the window handle

    // Set the window ownership to the root PID
    NTSTATUS result = ConsoleControl::SetWindowOwner(pseudoConsoleWindowHandle, rootProcessId, 0);

    if (result == STATUS_SUCCESS) {
        // Ownership change was successful
        // Add your logic here if needed
    } else {
        // Ownership change failed
        // Handle the error as needed
    }

    // Your application logic continues

    return 0;
}
