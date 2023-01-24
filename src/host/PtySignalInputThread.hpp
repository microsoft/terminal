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

#include "outputStream.hpp"

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
        void CreatePseudoWindow();

    private:
        enum class PtySignal : unsigned short
        {
            ShowHideWindow = 1,
            ClearBuffer = 2,
            SetParent = 3,
            ResizeWindow = 8
        };

        struct ResizeWindowData
        {
            unsigned short sx;
            unsigned short sy;
        };

        struct ShowHideData
        {
            unsigned short show; // used as a bool, but passed as a ushort
        };

        struct SetParentData
        {
            uint64_t handle;
        };

        [[nodiscard]] HRESULT _InputThread() noexcept;
        [[nodiscard]] bool _GetData(_Out_writes_bytes_(cbBuffer) void* const pBuffer, const DWORD cbBuffer);
        void _DoResizeWindow(const ResizeWindowData& data);
        void _DoSetWindowParent(const SetParentData& data);
        void _DoClearBuffer() const;
        void _DoShowHide(const ShowHideData& data);
        void _Shutdown();

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;
        bool _consoleConnected;
        std::optional<ResizeWindowData> _earlyResize;
        std::optional<ShowHideData> _initialShowHide;
        ConhostInternalGetSet _api;

    public:
        std::optional<SetParentData> _earlyReparent;
    };
}
