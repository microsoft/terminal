// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "ObjectModel/GlobalSettingsModel.h"
#include "Utils.h"

namespace winrt::SettingsControl::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
        GlobalAppearance();

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

    private:
        ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
