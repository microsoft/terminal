// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleInputThread.hpp"
#include "ConIoSrvComm.hpp"
#include "ConsoleWindow.hpp"

#include "ConIoSrv.h"

#include "..\..\host\input.h"

#include "..\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::OneCore;

DWORD WINAPI ConsoleInputThreadProcOneCore(LPVOID /*lpParam*/)
{
    Globals& globals = ServiceLocator::LocateGlobals();
    ConIoSrvComm* const Server = ServiceLocator::LocateInputServices<ConIoSrvComm>();

    NTSTATUS Status = Server->Connect();

    if (NT_SUCCESS(Status))
    {
        USHORT DisplayMode = Server->GetDisplayMode();

        if (DisplayMode != CIS_DISPLAY_MODE_NONE)
        {
            // Create and set the console window.
            ConsoleWindow* const wnd = new (std::nothrow) ConsoleWindow();
            Status = NT_TESTNULL(wnd);

            if (NT_SUCCESS(Status))
            {
                LOG_IF_FAILED(ServiceLocator::SetConsoleWindowInstance(wnd));

                // The console's renderer should be created before we get here.
                FAIL_FAST_IF_NULL(globals.pRender);

                switch (DisplayMode)
                {
                case CIS_DISPLAY_MODE_BGFX:
                    Status = Server->InitializeBgfx();
                    break;

                case CIS_DISPLAY_MODE_DIRECTX:
                    Status = Server->InitializeWddmCon();
                    break;
                }

                if (NT_SUCCESS(Status))
                {
                    globals.getConsoleInformation().GetActiveOutputBuffer().RefreshFontWithRenderer();
                }

                globals.ntstatusConsoleInputInitStatus = Status;
                globals.hConsoleInputInitEvent.SetEvent();

                if (NT_SUCCESS(Status))
                {
                    try
                    {
                        // Start listening for input (returns on failure only).
                        // This will never return.
                        Server->ServiceInputPipe();
                    }
                    catch (...)
                    {
                        // If we couldn't set up the input thread, log and cleanup
                        // and go to headless mode instead.
                        LOG_CAUGHT_EXCEPTION();
                        Status = wil::ResultFromCaughtException();
                        Server->CleanupForHeadless(Status);
                    }
                }
            }
        }
        else
        {
            // Nothing to do input-wise, but we must let the rest of the console
            // continue.
            Server->CleanupForHeadless(Status);
        }
    }
    else
    {
        // If we get an access denied and couldn't connect to the coniosrv in CSRSS.exe.
        // that's OK. We're likely inside an AppContainer in a TAEF /runas:uap test.
        // We don't want AppContainered things to have access to the hardware devices directly
        // like coniosrv in CSRSS offers, so we "succeeded" and will let the IO thread know it
        // can continue.
        if (STATUS_ACCESS_DENIED == Status)
        {
            Status = STATUS_SUCCESS;
        }

        // Notify IO thread of our status.
        Server->CleanupForHeadless(Status);
    }

    return Status;
}

// Routine Description:
// - Starts the OneCore-specific console input thread.
HANDLE ConsoleInputThread::Start()
{
    HANDLE hThread = nullptr;
    DWORD dwThreadId = (DWORD)-1;

    hThread = CreateThread(nullptr,
                           0,
                           ConsoleInputThreadProcOneCore,
                           _pConIoSrvComm,
                           0,
                           &dwThreadId);
    if (hThread)
    {
        _hThread = hThread;
        _dwThreadId = dwThreadId;
    }

    return hThread;
}

ConIoSrvComm* ConsoleInputThread::GetConIoSrvComm()
{
    return _pConIoSrvComm;
}
