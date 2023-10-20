/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VtInputThread.hpp

Abstract:
- Defines methods that wrap the thread that reads VT input from a pipe and
  feeds it into the console's input buffer.

Author(s):
- Mike Griese (migrie) 15 Aug 2017
--*/
#pragma once

#include "../terminal/parser/StateMachine.hpp"

namespace Microsoft::Console
{
    class VtInputThread
    {
    public:
        VtInputThread(_In_ wil::unique_hfile hPipe, const bool inheritCursor);

        [[nodiscard]] HRESULT Start();
        static DWORD WINAPI StaticVtInputThreadProc(_In_ LPVOID lpParameter);
        void DoReadInput(const bool throwOnFail);
        void SetLookingForDSR(const bool looking) noexcept;

    private:
        [[nodiscard]] HRESULT _HandleRunInput(const std::string_view u8Str);
        void _InputThread();

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;

        bool _exitRequested;

        std::function<void(bool)> _pfnSetLookingForDSR;

        std::unique_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _pInputStateMachine;
        til::u8state _u8State;
    };
}
