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
        virtual ~IConsoleInputThread() = default;
        virtual HANDLE Start() = 0;

        HANDLE GetHandle() noexcept { return _hThread; }
        DWORD GetThreadId() noexcept { return _dwThreadId; }

    protected:
        // Prevent accidental copies.
        IConsoleInputThread(const IConsoleInputThread&) = delete;
        IConsoleInputThread& operator=(const IConsoleInputThread&) = delete;

        // .ctor
        IConsoleInputThread() noexcept :
            _hThread(nullptr),
            _dwThreadId(gsl::narrow_cast<DWORD>(-1)) {}

        // Protected Variables
        HANDLE _hThread;
        DWORD _dwThreadId;
    };
}
