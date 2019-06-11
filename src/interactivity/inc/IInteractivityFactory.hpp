/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IInteractivityFactory.hpp

Abstract:
- Defines methods for a factory class that picks the implementation of
  interfaces depending on whether the console is running on OneCore or a larger
  edition of Windows with all the requisite API's to run the full console.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "IConsoleControl.hpp"
#include "IConsoleInputThread.hpp"

#include "IHighDpiApi.hpp"
#include "IWindowMetrics.hpp"
#include "IAccessibilityNotifier.hpp"
#include "ISystemConfigurationProvider.hpp"
#include "IInputServices.hpp"

#include <memory>

namespace Microsoft::Console::Interactivity
{
    class IInteractivityFactory
    {
    public:
        virtual ~IInteractivityFactory() = 0;
        [[nodiscard]] virtual NTSTATUS CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control) = 0;
        [[nodiscard]] virtual NTSTATUS CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread) = 0;

        [[nodiscard]] virtual NTSTATUS CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api) = 0;
        [[nodiscard]] virtual NTSTATUS CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics) = 0;
        [[nodiscard]] virtual NTSTATUS CreateAccessibilityNotifier(_Inout_ std::unique_ptr<IAccessibilityNotifier>& notifier) = 0;
        [[nodiscard]] virtual NTSTATUS CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider) = 0;
        [[nodiscard]] virtual NTSTATUS CreateInputServices(_Inout_ std::unique_ptr<IInputServices>& services) = 0;

        [[nodiscard]] virtual NTSTATUS CreatePseudoWindow(HWND& hwnd) = 0;
    };

    inline IInteractivityFactory::~IInteractivityFactory() {}
}
