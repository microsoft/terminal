// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Globals.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Globals : GlobalsT<Globals>
    {
        Globals();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Globals);
}
