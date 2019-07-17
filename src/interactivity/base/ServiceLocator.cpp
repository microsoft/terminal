// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\inc\ServiceLocator.hpp"

#include "InteractivityFactory.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;

#pragma region Private Static Member Initialization

std::unique_ptr<IInteractivityFactory> ServiceLocator::s_interactivityFactory;
std::unique_ptr<IConsoleControl> ServiceLocator::s_consoleControl;
std::unique_ptr<IConsoleInputThread> ServiceLocator::s_consoleInputThread;
std::unique_ptr<IWindowMetrics> ServiceLocator::s_windowMetrics;
std::unique_ptr<IAccessibilityNotifier> ServiceLocator::s_accessibilityNotifier;
std::unique_ptr<IHighDpiApi> ServiceLocator::s_highDpiApi;
std::unique_ptr<ISystemConfigurationProvider> ServiceLocator::s_systemConfigurationProvider;
std::unique_ptr<IInputServices> ServiceLocator::s_inputServices;

IConsoleWindow* ServiceLocator::s_consoleWindow = nullptr;

Globals ServiceLocator::s_globals;

bool ServiceLocator::s_pseudoWindowInitialized = false;
wil::unique_hwnd ServiceLocator::s_pseudoWindow = 0;

#pragma endregion

#pragma region Public Methods

void ServiceLocator::RundownAndExit(const HRESULT hr)
{
    // MSFT:15506250
    // In VT I/O Mode, a client application might die before we've rendered
    //      the last bit of text they've emitted. So give the VtRenderer one
    //      last chance to paint before it is killed.
    if (s_globals.pRender)
    {
        s_globals.pRender->TriggerTeardown();
    }

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

    if (s_inputServices.get() != nullptr)
    {
        s_inputServices.reset(nullptr);
    }

    TerminateProcess(GetCurrentProcess(), hr);
}

#pragma region Creation Methods

[[nodiscard]] NTSTATUS ServiceLocator::CreateConsoleInputThread(_Outptr_result_nullonfailure_ IConsoleInputThread** thread)
{
    NTSTATUS status = STATUS_SUCCESS;

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
        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateConsoleInputThread(s_consoleInputThread);

            if (NT_SUCCESS(status))
            {
                *thread = s_consoleInputThread.get();
            }
        }
    }

    return status;
}

#pragma endregion

#pragma region Set Methods

[[nodiscard]] NTSTATUS ServiceLocator::SetConsoleWindowInstance(_In_ IConsoleWindow* window)
{
    NTSTATUS status = STATUS_SUCCESS;

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
        s_consoleWindow = window;
    }

    return status;
}

#pragma endregion

#pragma region Location Methods

IConsoleWindow* ServiceLocator::LocateConsoleWindow()
{
    return s_consoleWindow;
}

IConsoleControl* ServiceLocator::LocateConsoleControl()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_consoleControl)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
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
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_highDpiApi)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateHighDpiApi(s_highDpiApi);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_highDpiApi.get();
}

IWindowMetrics* ServiceLocator::LocateWindowMetrics()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_windowMetrics)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateWindowMetrics(s_windowMetrics);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_windowMetrics.get();
}

IAccessibilityNotifier* ServiceLocator::LocateAccessibilityNotifier()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_accessibilityNotifier)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateAccessibilityNotifier(s_accessibilityNotifier);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_accessibilityNotifier.get();
}

ISystemConfigurationProvider* ServiceLocator::LocateSystemConfigurationProvider()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_systemConfigurationProvider)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateSystemConfigurationProvider(s_systemConfigurationProvider);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_systemConfigurationProvider.get();
}

IInputServices* ServiceLocator::LocateInputServices()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!s_inputServices)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            status = s_interactivityFactory->CreateInputServices(s_inputServices);
        }
    }

    LOG_IF_NTSTATUS_FAILED(status);

    return s_inputServices.get();
}

Globals& ServiceLocator::LocateGlobals()
{
    return s_globals;
}

// Method Description:
// - Retrieves the pseudo console window, or attempts to instantiate one.
// Arguments:
// - <none>
// Return Value:
// - a reference to the pseudoconsole window.
HWND ServiceLocator::LocatePseudoWindow()
{
    NTSTATUS status = STATUS_SUCCESS;
    if (!s_pseudoWindowInitialized)
    {
        if (s_interactivityFactory.get() == nullptr)
        {
            status = ServiceLocator::LoadInteractivityFactory();
        }

        if (NT_SUCCESS(status))
        {
            HWND hwnd;
            status = s_interactivityFactory->CreatePseudoWindow(hwnd);
            s_pseudoWindow.reset(hwnd);
        }
        s_pseudoWindowInitialized = true;
    }
    LOG_IF_NTSTATUS_FAILED(status);
    return s_pseudoWindow.get();
}

#pragma endregion

#pragma endregion

#pragma region Private Methods

[[nodiscard]] NTSTATUS ServiceLocator::LoadInteractivityFactory()
{
    NTSTATUS status = STATUS_SUCCESS;

    if (s_interactivityFactory.get() == nullptr)
    {
        s_interactivityFactory = std::make_unique<InteractivityFactory>();
        status = NT_TESTNULL(s_interactivityFactory.get());
    }
    return status;
}

#pragma endregion
