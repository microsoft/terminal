// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AddProfile.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct AddProfile : AddProfileT<AddProfile>
    {
        AddProfile();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
