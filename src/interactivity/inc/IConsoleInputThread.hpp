/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IConsoleInputThread.hpp

Abstract:
- Defines methods that wrap the thread that reads input from the keyboard and
  feeds it into the console's input buffer.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IConsoleInputThread
    {
    public:
        virtual ~IConsoleInputThread() = 0;
        virtual HANDLE Start() = 0;

        HANDLE GetHandle() { return _hThread; }
        DWORD GetThreadId() { return _dwThreadId; }

    protected:
        // Prevent accidental copies.
        IConsoleInputThread(IConsoleInputThread const&) = delete;
        IConsoleInputThread& operator=(IConsoleInputThread const&) = delete;

        // .ctor
        IConsoleInputThread() :
            _hThread(nullptr),
            _dwThreadId((DWORD)(-1)) {}

        // Protected Variables
        HANDLE _hThread;
        DWORD _dwThreadId;
    };

    inline IConsoleInputThread::~IConsoleInputThread() {}
}
