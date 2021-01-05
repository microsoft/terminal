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
        ApplicationState(std::filesystem::path path) :
            _path{ path } {}

        static Microsoft::Terminal::Settings::Model::ApplicationState GetForCurrentApp();
        static void Reset();

        void Commit();

        bool ShowConfirmCloseAllTabs();
        void ShowConfirmCloseAllTabs(bool value);
        bool ShowConfirmLargePaste();
        void ShowConfirmLargePaste(bool value);
        bool ShowConfirmMultiLinePaste();
        void ShowConfirmMultiLinePaste(bool value);

    private:
        static winrt::com_ptr<ApplicationState> LoadAll();
        void LayerJson(const Json::Value& value);
        void Invalidate() { _invalidated = true; }
        bool _invalidated{ false };
        std::filesystem::path _path{};
    };
}
namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ApplicationState);
}
