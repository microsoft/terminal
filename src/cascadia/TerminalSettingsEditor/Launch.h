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

        void OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e);

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
