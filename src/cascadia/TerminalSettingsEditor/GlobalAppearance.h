// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "ObjectModel/GlobalSettingsModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
        GlobalAppearance();

        Model::GlobalSettingsModel GlobalSettingsModel();

        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalAppSettings();

    private:
        Model::GlobalSettingsModel m_globalSettingsModel{ nullptr };

        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings _GlobalAppSettings{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
