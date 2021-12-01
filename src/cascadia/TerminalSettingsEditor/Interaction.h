// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "InteractionPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct InteractionPageNavigationState : InteractionPageNavigationStateT<InteractionPageNavigationState>
    {
    public:
        InteractionPageNavigationState(const Editor::GlobalSettingsViewModel& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr)
    };

    struct Interaction : public HasScrollViewer<Interaction>, InteractionT<Interaction>
    {
        Interaction();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        WINRT_PROPERTY(Editor::GlobalSettingsViewModel, Globals, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
