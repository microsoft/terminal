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
        void Reload();
        void Reset();

        void Commit();

        GETSET_PROPERTY(bool, CloseAllTabsWarningDismissed, false);
        GETSET_PROPERTY(bool, LargePasteWarningDismissed, false);
        GETSET_PROPERTY(bool, MultiLinePasteWarningDismissed, false);

    private:
        void _load();
        void _delete();

        void LayerJson(const Json::Value& value);
        Json::Value ToJson() const;

        bool _invalidated{ false };
        std::filesystem::path _path{};
    };
}
namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ApplicationState);
}
