// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Globals.g.h"
#include "Utils.h"

namespace winrt::SettingsControl::implementation
{
    struct Globals : GlobalsT<Globals>
    {
        Globals();
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(Globals);
}
