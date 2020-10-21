// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();
        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
