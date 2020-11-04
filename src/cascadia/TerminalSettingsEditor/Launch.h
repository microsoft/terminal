// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Launch.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Launch : LaunchT<Launch>
    {
    public:
        Launch();

        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
