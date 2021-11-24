// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalSettingsViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "..\TerminalSettingsModel\MTSMSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalSettingsViewModel : GlobalSettingsViewModelT<GlobalSettingsViewModel>, ViewModelHelper<GlobalSettingsViewModel>
    {
    public:
        GlobalSettingsViewModel(const Model::GlobalAppSettings& globalSettings);

        void SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue);
        _GET_SETTING_FUNC(_globals, DefaultProfile);
        _SET_SETTING_FUNC(_globals, DefaultProfile);
        _CLEAR_SETTING_FUNC(_globals, Language);

#define GLOBAL_SETTINGS_INITIALIZE(type, name, ...) \
    PERMANENT_OBSERVABLE_PROJECTED_SETTING(_globals, name)
        MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_INITIALIZE)
#undef GLOBAL_SETTINGS_INITIALIZE

    private:
        Model::GlobalAppSettings _globals;
    };
};
