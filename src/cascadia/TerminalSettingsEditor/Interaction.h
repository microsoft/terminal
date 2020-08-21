// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

    private:
        ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
