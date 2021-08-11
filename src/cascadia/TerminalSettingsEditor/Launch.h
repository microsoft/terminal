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

    template<typename T>
    struct HasScrollViewer
    {
        void ViewChanging(winrt::Windows::Foundation::IInspectable const& sender, const winrt::Windows::UI::Xaml::Controls::ScrollViewerViewChangingEventArgs& e)
        {
            DismissAllPopups(XamlRoot());
        }
    };

    struct Launch : public HasScrollViewer<Launch>, LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        // void ViewChanging(winrt::Windows::Foundation::IInspectable const& sender, const winrt::Windows::UI::Xaml::Controls::ScrollViewerViewChangingEventArgs& e);

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);

        WINRT_PROPERTY(Editor::LaunchPageNavigationState, State, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, State().Settings().GlobalSettings, LaunchMode);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, State().Settings().GlobalSettings, WindowingBehavior);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
