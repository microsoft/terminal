// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();

        Model::GlobalSettingsModel GlobalSettingsModel();

    private:
        Model::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
