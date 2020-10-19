// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();
        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
