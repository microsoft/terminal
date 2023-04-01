// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

// MidiAudio
#include <mmeapi.h>
#include <dsound.h>

#include "../inc/ServiceLocator.hpp"

#include "InteractivityFactory.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;

#pragma region Private Static Member Initialization

std::unique_ptr<IInteractivityFactory> ServiceLocator::s_interactivityFactory;
std::unique_ptr<IConsoleControl> ServiceLocator::s_consoleControl;
std::unique_ptr<IConsoleInputThread> ServiceLocator::s_consoleInputThread;
std::unique_ptr<IConsoleWindow> ServiceLocator::s_consoleWindow;
std::unique_ptr<IWindowMetrics> ServiceLocator::s_windowMetrics;
std::unique_ptr<IAccessibilityNotifier> ServiceLocator::s_accessibilityNotifier;
std::unique_ptr<IHighDpiApi> ServiceLocator::s_highDpiApi;
std::unique_ptr<ISystemConfigurationProvider> ServiceLocator::s_systemConfigurationProvider;
void (*ServiceLocator::s_oneCoreTeardownFunction)() = nullptr;

Globals ServiceLocator::s_globals;

bool ServiceLocator::s_pseudoWindowInitialized = false;
wil::unique_hwnd ServiceLocator::s_pseudoWindow = nullptr;

#pragma endregion

#pragma region Public Methods

void ServiceLocator::SetOneCoreTeardownFunction(void (*pfn)()) noexcept
{
    FAIL_FAST_IF(nullptr != s_oneCoreTeardownFunction);
    s_oneCoreTeardownFunction = pfn;
}

void ServiceLocator::RundownAndExit(const HRESULT hr)
{
    // The TriggerTeardown() call below depends on the render thread being able to acquire the
    // console lock, so that it can safely progress with flushing the last frame. Since there's no
    // coming back from this function (it's [[noreturn]]), it's safe to unlock the console here.
    auto& gci = s_globals.getConsoleInformation();
    while (gci.IsConsoleLocked())
    {
        gci.UnlockConsole();
    }

    // MSFT:40146639
    //   The premise of this function is that 1 thread enters and 0 threads leave alive.
    //   We need to prevent anyone from calling us until we actually ExitProcess(),
    //   so that we don't TriggerTeardown() twice. LockConsole() can't be used here,
    //   because doing so would prevent the render thread from progressing.
    static std::atomic<bool> locked;
    if (locked.exchange(true, std::memory_order_relaxed))
    {
        // If we reach this point, another thread is already in the process of exiting.
        // There's a lot of ways to suspend ourselves until we exit, one of which is "sleep forever".
        Sleep(INFINITE);
    }

    // MSFT:15506250
    // In VT I/O Mode, a client application might die before we've rendered
    //      the last bit of text they've emitted. So give the VtRenderer one
    //      last chance to paint before it is killed.
    if (s_globals.pRender)
    {
        s_globals.pRender->TriggerTeardown();
    }

    // MSFT:40226902 - HOTFIX shutdown on OneCore, by leaking the renderer, thereby
    // reducing the change for existing race conditions to turn into deadlocks.
#ifndef NDEBUG
    // By locking the console, we ensure no background tasks are accessing the
    // classes we're going to destruct down below (for instance: CursorBlinker).
    s_globals.getConsoleInformation().LockConsole();
#endif

    // A History Lesson from MSFT: 13576341:
    // We introduced RundownAndExit to give services that hold onto important handles
    // an opportunity to let those go when we decide to exit from the console for various reasons.
    // This was because Console IO Services (ConIoSvcComm) on OneCore editions was holding onto
    // pipe and ALPC handles to talk to CSRSS ConIoSrv.dll to broker which console got display/keyboard control.
    // If we simply run straight into TerminateProcess, those handles aren't necessarily released right away.
    // The terminate operation can have a rundown period of time where APCs are serviced (such as from
    // a DirectX kernel callback/flush/cleanup) that can take substantially longer than we expect (several whole seconds).
    // This rundown happens before the final destruction of any outstanding handles or resources.
    // If someone is waiting on one of those handles or resources outside our process, they're stuck waiting
    // for our terminate rundown and can't continue execution until we're done.
    // We don't want to have other execution in the system get stuck, so this is a great
    // place to clean up and notify any objects or threads in the system that have to cleanup safely before
    // we head into TerminateProcess and tear everything else down less gracefully.

    // TODO: MSFT: 14397093 - Expand graceful rundown beyond just the Hot Bug input services case.

    // MSFT:40226902 - HOTFIX shutdown on OneCore, by leaking the renderer, thereby
    // reducing the change for existing race conditions to turn into deadlocks.
#ifndef NDEBUG
    delete s_globals.pRender;
    s_globals.pRender = nullptr;
#endif

    if (s_oneCoreTeardownFunction)
    {
        s_oneCoreTeardownFunction();
    }

    // MSFT:40226902 - HOTFIX shutdown on OneCore, by leaking the renderer, thereby
    // reducing the change for existing race conditions to turn into deadlocks.
#ifndef NDEBUG
    s_consoleWindow.reset(nullptr);
#endif

    ExitProcess(hr);
}

#pragma region Creation Methods

[[nodiscard]] NTSTATUS ServiceLocator::CreateConsoleInputThread(_Outptr_result_nullonfailure_ IConsoleInputThread** thread)
{
    auto status = STATUS_SUCCESS;

    if (s_consoleInputThread)
    {
        status = STATUS_INVALID_HANDLE;
    }
    else
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }
        if (SUCCEEDED_NTSTATUS(status))
        {
            status = s_interactivityFactory->CreateConsoleInputThread(s_consoleInputThread);

            if (SUCCEEDED_NTSTATUS(status))
            {
                *thread = s_consoleInputThread.get();
            }
        }
    }

    return status;
}

[[nodiscard]] HRESULT ServiceLocator::CreateAccessibilityNotifier()
{
    // Can't create if we've already created.
    if (s_accessibilityNotifier)
    {
        return E_UNEXPECTED;
    }

    if (!s_interactivityFactory)
    {
        RETURN_IF_NTSTATUS_FAILED(ServiceLocator::LoadInteractivityFactory());
    }

    RETURN_IF_NTSTATUS_FAILED(s_interactivityFactory->CreateAccessibilityNotifier(s_accessibilityNotifier));

    return S_OK;
}

#pragma endregion

#pragma region Set Methods

[[nodiscard]] NTSTATUS ServiceLocator::SetConsoleControlInstance(_In_ std::unique_ptr<IConsoleControl>&& control)
{
    if (s_consoleControl)
    {
        NT_RETURN_NTSTATUS(STATUS_INVALID_HANDLE);
    }
    else if (!control)
    {
        NT_RETURN_NTSTATUS(STATUS_INVALID_PARAMETER);
    }
    else
    {
        s_consoleControl = std::move(control);
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ServiceLocator::SetConsoleWindowInstance(_In_ IConsoleWindow* window)
{
    auto status = STATUS_SUCCESS;

    if (s_consoleWindow)
    {
        status = STATUS_INVALID_HANDLE;
    }
    else if (!window)
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else
    {
        s_consoleWindow.reset(window);
    }

    return status;
}

#pragma endregion

#pragma region Location Methods

IConsoleWindow* ServiceLocator::LocateConsoleWindow()
{
    return s_consoleWindow.get();
}

IConsoleControl* ServiceLocator::LocateConsoleControl()
{
    auto status = STATUS_SUCCESS;

    if (!s_consoleControl)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            status = s_interactivityFactory->CreateConsoleControl(s_consoleControl);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_consoleControl.get();
}

IConsoleInputThread* ServiceLocator::LocateConsoleInputThread()
{
    return s_consoleInputThread.get();
}

IHighDpiApi* ServiceLocator::LocateHighDpiApi()
{
    auto status = STATUS_SUCCESS;

    if (!s_highDpiApi)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            status = s_interactivityFactory->CreateHighDpiApi(s_highDpiApi);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_highDpiApi.get();
}

IWindowMetrics* ServiceLocator::LocateWindowMetrics()
{
    auto status = STATUS_SUCCESS;

    if (!s_windowMetrics)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            status = s_interactivityFactory->CreateWindowMetrics(s_windowMetrics);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_windowMetrics.get();
}

IAccessibilityNotifier* ServiceLocator::LocateAccessibilityNotifier()
{
    return s_accessibilityNotifier.get();
}

ISystemConfigurationProvider* ServiceLocator::LocateSystemConfigurationProvider()
{
    auto status = STATUS_SUCCESS;

    if (!s_systemConfigurationProvider)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            status = s_interactivityFactory->CreateSystemConfigurationProvider(s_systemConfigurationProvider);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_systemConfigurationProvider.get();
}

Globals& ServiceLocator::LocateGlobals()
{
    return s_globals;
}

// Method Description:
// - Retrieves the pseudo console window, or attempts to instantiate one.
// Arguments:
// - owner: (defaults to 0 `HWND_DESKTOP`) the HWND that should be the initial
//   owner of the pseudo window.
// Return Value:
// - a reference to the pseudoconsole window.
HWND ServiceLocator::LocatePseudoWindow(const HWND owner)
{
    auto status = STATUS_SUCCESS;
    if (!s_pseudoWindowInitialized)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            HWND hwnd;
            status = s_interactivityFactory->CreatePseudoWindow(hwnd, owner);
            s_pseudoWindow.reset(hwnd);
        }

        s_pseudoWindowInitialized = true;
    }
    LOG_IF_NTSTATUS_FAILED(status);
    return s_pseudoWindow.get();
}

#pragma endregion

#pragma region Private Methods

[[nodiscard]] NTSTATUS ServiceLocator::LoadInteractivityFactory()
{
    auto status = STATUS_SUCCESS;

    if (s_interactivityFactory.get() == nullptr)
    {
        s_interactivityFactory = std::make_unique<InteractivityFactory>();
        status = NT_TESTNULL(s_interactivityFactory.get());
    }
    return status;
}

#pragma endregion
