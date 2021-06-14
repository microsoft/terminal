/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- HostSignalInputThread.hpp

Abstract:
- Defines methods that wrap the thread that will wait for signals
  from a delegated console host to this "owner" console.

Author(s):
- Michael Niksa (miniksa) 10 Jun 2021

Notes:
- Sourced from `PtySignalInputThread`
--*/
#pragma once

namespace Microsoft::Console
{
    class HostSignalInputThread final
    {
    public:
        HostSignalInputThread(_In_ wil::unique_hfile&& hPipe);
        ~HostSignalInputThread();

        [[nodiscard]] HRESULT Start() noexcept;
        static DWORD WINAPI StaticThreadProc(_In_ LPVOID lpParameter);

        // Prevent copying and assignment.
        HostSignalInputThread(const HostSignalInputThread&) = delete;
        HostSignalInputThread& operator=(const HostSignalInputThread&) = delete;

        void ConnectConsole() noexcept;

    private:
        template<typename T>
        T _ReaderHelper();
        [[nodiscard]] HRESULT _InputThread();

        bool _GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer, const DWORD cbBuffer);
        bool _AdvanceReader(const DWORD cbBytes);
        void _Shutdown();

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;
        bool _consoleConnected;
    };
}
