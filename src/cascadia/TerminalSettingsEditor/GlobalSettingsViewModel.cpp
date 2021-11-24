// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalSettingsViewModel.h"
#include "GlobalSettingsViewModel.g.cpp"
#include "..\WinRTUtils\inc\Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalSettingsViewModel::GlobalSettingsViewModel(const Model::GlobalAppSettings& globalSettings) :
        _globals{ globalSettings } {}

    void GlobalSettingsViewModel::SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue)
    {
        _globals.SetInvertedDisableAnimationsValue(invertedDisableAnimationsValue);
        _NotifyChanges(L"DisableAnimations");
    }
}
