// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AddProfile.g.h"
#include "Utils.h"

namespace winrt::SettingsControl::implementation
{
    struct AddProfile : AddProfileT<AddProfile>
    {
        AddProfile();
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
