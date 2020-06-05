/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ServiceLocator.hpp

Abstract:
- Locates and holds instances of classes for which multiple implementations
  exist depending on API's available on the host OS.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "IInteractivityFactory.hpp"
#include "../types/IConsoleWindow.hpp"
#include "../../host/globals.h"

#include <memory>

#pragma hdrstop

using namespace Microsoft::Console::Types;

namespace Microsoft::Console::Interactivity
{
    class ServiceLocator final
    {
    public:
        static void RundownAndExit(const HRESULT hr);

        // N.B.: Location methods without corresponding creation methods
        //       automatically create the singleton object on demand.
        //       In case the on-demand creation fails, the return value
        //       is nullptr and a message is logged.

        static IAccessibilityNotifier* LocateAccessibilityNotifier();

        static IConsoleControl* LocateConsoleControl();
        template<typename T>
        static T* LocateConsoleControl()
        {
            return static_cast<T*>(LocateConsoleControl());
        }

        [[nodiscard]] static NTSTATUS CreateConsoleInputThread(_Outptr_result_nullonfailure_ IConsoleInputThread** thread);
        static IConsoleInputThread* LocateConsoleInputThread();
        template<typename T>
        static T* LocateConsoleInputThread()
        {
            return static_cast<T*>(LocateConsoleInputThread());
        }

        [[nodiscard]] static NTSTATUS SetConsoleWindowInstance(_In_ IConsoleWindow* window);
        static IConsoleWindow* LocateConsoleWindow();
        template<typename T>
        static T* LocateConsoleWindow()
        {
            return static_cast<T*>(s_consoleWindow);
        }

        static IWindowMetrics* LocateWindowMetrics();
        template<typename T>
        static T* LocateWindowMetrics()
        {
            return static_cast<T*>(LocateWindowMetrics());
        }

        static IHighDpiApi* LocateHighDpiApi();
        template<typename T>
        static T* LocateHighDpiApi()
        {
            return static_cast<T*>(LocateHighDpiApi());
        }

        static IInputServices* LocateInputServices();
        template<typename T>
        static T* LocateInputServices()
        {
            return static_cast<T*>(LocateInputServices());
        }

        static ISystemConfigurationProvider* LocateSystemConfigurationProvider();

        static Globals& LocateGlobals();

        static HWND LocatePseudoWindow();

    protected:
        ServiceLocator(ServiceLocator const&) = delete;
        ServiceLocator& operator=(ServiceLocator const&) = delete;

    private:
        [[nodiscard]] static NTSTATUS LoadInteractivityFactory();

        static std::unique_ptr<IInteractivityFactory> s_interactivityFactory;

        static std::unique_ptr<IAccessibilityNotifier> s_accessibilityNotifier;
        static std::unique_ptr<IConsoleControl> s_consoleControl;
        static std::unique_ptr<IConsoleInputThread> s_consoleInputThread;
        // TODO: MSFT 15344939 - some implementations of IConsoleWindow are currently singleton
        // classes so we can't own a pointer to them here. fix this so s_consoleWindow can follow the
        // pattern of the rest of the service interface pointers.
        static IConsoleWindow* s_consoleWindow;
        static std::unique_ptr<IWindowMetrics> s_windowMetrics;
        static std::unique_ptr<IHighDpiApi> s_highDpiApi;
        static std::unique_ptr<ISystemConfigurationProvider> s_systemConfigurationProvider;
        static std::unique_ptr<IInputServices> s_inputServices;

        static Globals s_globals;
        static bool s_pseudoWindowInitialized;
        static wil::unique_hwnd s_pseudoWindow;
    };
}
