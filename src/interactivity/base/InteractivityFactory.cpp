// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "InteractivityFactory.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\onecore\AccessibilityNotifier.hpp"
#include "..\onecore\ConsoleControl.hpp"
#include "..\onecore\ConsoleInputThread.hpp"
#include "..\onecore\ConsoleWindow.hpp"
#include "..\onecore\ConIoSrvComm.hpp"
#include "..\onecore\SystemConfigurationProvider.hpp"
#include "..\onecore\WindowMetrics.hpp"
#endif

#include "..\win32\AccessibilityNotifier.hpp"
#include "..\win32\ConsoleControl.hpp"
#include "..\win32\ConsoleInputThread.hpp"
#include "..\win32\InputServices.hpp"
#include "..\win32\WindowDpiApi.hpp"
#include "..\win32\WindowMetrics.hpp"
#include "..\win32\SystemConfigurationProvider.hpp"

#pragma hdrstop

using namespace std;
using namespace Microsoft::Console::Interactivity;

#pragma region Public Methods

[[nodiscard]] NTSTATUS InteractivityFactory::CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IConsoleControl> newControl;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newControl = std::make_unique<Microsoft::Console::Interactivity::Win32::ConsoleControl>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newControl = std::make_unique<Microsoft::Console::Interactivity::OneCore::ConsoleControl>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            control.swap(newControl);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IConsoleInputThread> newThread;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newThread = std::make_unique<Microsoft::Console::Interactivity::Win32::ConsoleInputThread>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newThread = std::make_unique<Microsoft::Console::Interactivity::OneCore::ConsoleInputThread>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            thread.swap(newThread);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IHighDpiApi> newApi;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newApi = std::make_unique<Microsoft::Console::Interactivity::Win32::WindowDpiApi>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newApi.reset();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            api.swap(newApi);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IWindowMetrics> newMetrics;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newMetrics = std::make_unique<Microsoft::Console::Interactivity::Win32::WindowMetrics>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newMetrics = std::make_unique<Microsoft::Console::Interactivity::OneCore::WindowMetrics>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            metrics.swap(newMetrics);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateAccessibilityNotifier(_Inout_ std::unique_ptr<IAccessibilityNotifier>& notifier)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IAccessibilityNotifier> newNotifier;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newNotifier = std::make_unique<Microsoft::Console::Interactivity::Win32::AccessibilityNotifier>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newNotifier = std::make_unique<Microsoft::Console::Interactivity::OneCore::AccessibilityNotifier>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            notifier.swap(newNotifier);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<ISystemConfigurationProvider> NewProvider;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                NewProvider = std::make_unique<Microsoft::Console::Interactivity::Win32::SystemConfigurationProvider>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                NewProvider = std::make_unique<Microsoft::Console::Interactivity::OneCore::SystemConfigurationProvider>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            provider.swap(NewProvider);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateInputServices(_Inout_ std::unique_ptr<IInputServices>& services)
{
    NTSTATUS status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (NT_SUCCESS(status))
    {
        std::unique_ptr<IInputServices> newServices;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newServices = std::make_unique<Microsoft::Console::Interactivity::Win32::InputServices>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newServices = std::make_unique<Microsoft::Console::Interactivity::OneCore::ConIoSrvComm>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (NT_SUCCESS(status))
        {
            services.swap(newServices);
        }
    }

    return status;
}

// Method Description:
// - Attempts to instantiate a "pseudo window" for when we're operating in
//      pseudoconsole mode. There are some tools (cygwin & derivatives) that use
//      the GetConsoleWindow API to uniquely identify console sessions. This
//      function is used to create an invisible window for that scenario, so
//      that GetConsoleWindow returns a real value.
// Arguments:
// - hwnd: Receives the value of the newly created window's HWND.
// Return Value:
// - STATUS_SUCCESS on success, otherwise an appropriate error.
[[nodiscard]] NTSTATUS InteractivityFactory::CreatePseudoWindow(HWND& hwnd)
{
    hwnd = 0;
    ApiLevel level;
    NTSTATUS status = ApiDetector::DetectNtUserWindow(&level);
    ;
    if (NT_SUCCESS(status))
    {
        try
        {
            static const wchar_t* const PSEUDO_WINDOW_CLASS = L"PseudoConsoleWindow";
            WNDCLASS pseudoClass{ 0 };
            switch (level)
            {
            case ApiLevel::Win32:
                pseudoClass.lpszClassName = PSEUDO_WINDOW_CLASS;
                pseudoClass.lpfnWndProc = DefWindowProc;
                RegisterClass(&pseudoClass);
                // Attempt to create window
                hwnd = CreateWindowExW(
                    0, PSEUDO_WINDOW_CLASS, nullptr, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, HWND_DESKTOP, nullptr, nullptr, nullptr);
                if (hwnd == nullptr)
                {
                    DWORD const gle = GetLastError();
                    status = NTSTATUS_FROM_WIN32(gle);
                }
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                hwnd = 0;
                status = STATUS_SUCCESS;
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }
    }

    return status;
}
#pragma endregion
