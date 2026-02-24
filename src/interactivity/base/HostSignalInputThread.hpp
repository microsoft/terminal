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
        HostSignalInputThread(wil::unique_hfile&& hPipe);
        ~HostSignalInputThread();

        [[nodiscard]] HRESULT Start() noexcept;
        static DWORD WINAPI StaticThreadProc(LPVOID lpParameter);

        // Prevent copying and assignment.
        HostSignalInputThread(const HostSignalInputThread&) = delete;
        HostSignalInputThread& operator=(const HostSignalInputThread&) = delete;

    private:
        template<typename T>
        T _ReceiveTypedPacket();
        [[nodiscard]] HRESULT _InputThread();

        bool _GetData(std::span<std::byte> buffer);
        bool _AdvanceReader(DWORD byteCount);
        void _Shutdown();

        DWORD _dwThreadId;
        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
    };
}
