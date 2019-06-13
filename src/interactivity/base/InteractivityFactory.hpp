// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "precomp.h"

#include "ApiDetector.hpp"

#include "..\inc\IInteractivityFactory.hpp"

#include <map>

#pragma hdrstop

namespace Microsoft::Console::Interactivity
{
    class InteractivityFactory final : public IInteractivityFactory
    {
    public:
        [[nodiscard]] NTSTATUS CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control);
        [[nodiscard]] NTSTATUS CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread);

        [[nodiscard]] NTSTATUS CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api);
        [[nodiscard]] NTSTATUS CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics);
        [[nodiscard]] NTSTATUS CreateAccessibilityNotifier(_Inout_ std::unique_ptr<IAccessibilityNotifier>& notifier);
        [[nodiscard]] NTSTATUS CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider);
        [[nodiscard]] NTSTATUS CreateInputServices(_Inout_ std::unique_ptr<IInputServices>& services);

        [[nodiscard]] NTSTATUS CreatePseudoWindow(HWND& hwnd);
    };
}
