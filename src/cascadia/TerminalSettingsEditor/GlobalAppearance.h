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
        GlobalAppearancePageNavigationState(const Editor::GlobalSettingsViewModel& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr)
    };

    struct GlobalAppearance : public HasScrollViewer<GlobalAppearance>, GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
