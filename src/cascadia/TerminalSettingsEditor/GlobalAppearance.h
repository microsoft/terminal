// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();

        void OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e);

        GETSET_PROPERTY(Model::GlobalAppSettings, GlobalSettings, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(Theme, winrt::Windows::UI::Xaml::ElementTheme, GlobalSettings, Theme);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, GlobalSettings, TabWidthMode);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
