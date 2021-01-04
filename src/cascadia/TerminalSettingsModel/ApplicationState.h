/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApplicationState.h

--*/
#pragma once

#include "ApplicationState.g.h"
#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ApplicationState : ApplicationStateT<ApplicationState>
    {
        ApplicationState() = default;

        static Microsoft::Terminal::Settings::Model::ApplicationState GetForCurrentApp();
        static void Reset();

        bool ShowConfirmCloseAllTabs();
        void ShowConfirmCloseAllTabs(bool value);
        bool ShowConfirmLargePaste();
        void ShowConfirmLargePaste(bool value);
        bool ShowConfirmMultiLinePaste();
        void ShowConfirmMultiLinePaste(bool value);
    };
}
namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ApplicationState);
}
