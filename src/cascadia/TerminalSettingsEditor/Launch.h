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
        LaunchPageNavigationState(const Editor::GlobalSettingsViewModel& globals, const Model::CascadiaSettings& appSettings) :
            _Globals{ globals },
            _AppSettings{ appSettings } {}

        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr)
        WINRT_PROPERTY(Model::CascadiaSettings, AppSettings, nullptr);
    };

    struct Launch : public HasScrollViewer<Launch>, LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr);
        WINRT_PROPERTY(Model::CascadiaSettings, AppSettings, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
