// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "GlobalAppearancePageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearancePageNavigationState : GlobalAppearancePageNavigationStateT<GlobalAppearancePageNavigationState>
    {
    public:
        GlobalAppearancePageNavigationState(const Model::GlobalAppSettings& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Model::GlobalAppSettings, Globals, nullptr)
    };

    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::GlobalAppearancePageNavigationState, State, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(Theme, winrt::Windows::UI::Xaml::ElementTheme, State().Globals, Theme);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, State().Globals, TabWidthMode);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
