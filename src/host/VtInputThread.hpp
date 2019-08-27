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

#include "..\terminal\parser\StateMachine.hpp"
#include "utf8ToWideCharParser.hpp"

namespace Microsoft::Console
{
    class VtInputThread
    {
    public:
        VtInputThread(wil::unique_hfile hPipe,
                      wil::shared_event shutdownEvent,
                      const bool inheritCursor);

        ~VtInputThread();

        [[nodiscard]] HRESULT Start();
        static DWORD WINAPI StaticVtInputThreadProc(_In_ LPVOID lpParameter);

        [[nodiscard]] HRESULT DoReadInput();

    private:
        
        [[nodiscard]] HRESULT _HandleRunInput(_In_reads_(cch) const byte* const charBuffer, const int cch);
        DWORD _InputThread();
        [[nodiscard]] HRESULT _ReadInput();

        wil::shared_event _shutdownEvent;
        std::future<void> _shutdownWatchdog;

        wil::unique_hfile _hFile;
        wil::unique_handle _hThread;
        DWORD _dwThreadId;

        std::unique_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _pInputStateMachine;
        Utf8ToWideCharParser _utf8Parser;
    };
}
