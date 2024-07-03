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
        bool DoReadInput();
        bool IsLookingForDSR() const noexcept;

    private:
        static DWORD WINAPI StaticVtInputThreadProc(_In_ LPVOID lpParameter);
        void _InputThread();

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;

        std::unique_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _pInputStateMachine;
        til::u8state _u8State;
        std::wstring _wstr;
    };
}
