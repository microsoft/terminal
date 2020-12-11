// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Launch.g.h"
#include "LaunchPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct LaunchPageNavigationState : LaunchPageNavigationStateT<LaunchPageNavigationState>
    {
    public:
        LaunchPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
    };

    struct Launch : LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);

        GETSET_PROPERTY(Editor::LaunchPageNavigationState, State, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, State().Settings().GlobalSettings, LaunchMode);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
