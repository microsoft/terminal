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

    private:
        Model::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
