/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PtySignalInputThread.hpp

Abstract:
- Defines methods that wrap the thread that will wait for Pty Signals
  if a Pty server (VT server) is running.

Author(s):
- Mike Griese (migrie) 15 Aug 2017
- Michael Niksa (miniksa) 19 Jan 2018
--*/
#pragma once

namespace Microsoft::Console
{
    class PtySignalInputThread final
    {
    public:
        PtySignalInputThread(_In_ wil::unique_hfile hPipe);
        ~PtySignalInputThread();

        [[nodiscard]] HRESULT Start() noexcept;
        static DWORD WINAPI StaticThreadProc(_In_ LPVOID lpParameter);

        // Prevent copying and assignment.
        PtySignalInputThread(const PtySignalInputThread&) = delete;
        PtySignalInputThread& operator=(const PtySignalInputThread&) = delete;

        void ConnectConsole() noexcept;

    private:
        [[nodiscard]] HRESULT _InputThread();
        bool _GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer, const DWORD cbBuffer);
        void _Shutdown();

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;
        bool _consoleConnected;
        std::unique_ptr<Microsoft::Console::VirtualTerminal::ConGetSet> _pConApi;
    };
}
