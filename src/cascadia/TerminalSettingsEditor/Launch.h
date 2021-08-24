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

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
    };

    struct Launch : public HasScrollViewer<Launch>, LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<IInspectable> DefaultProfiles() const;

        WINRT_PROPERTY(Editor::LaunchPageNavigationState, State, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, State().Settings().GlobalSettings, LaunchMode);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, State().Settings().GlobalSettings, WindowingBehavior);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
