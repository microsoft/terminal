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
        InteractionPageNavigationState(const Model::GlobalAppSettings& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Model::GlobalAppSettings, Globals, nullptr)
    };

    struct Interaction : public HasScrollViewer<Interaction>, InteractionT<Interaction>
    {
        Interaction();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::InteractionPageNavigationState, State, nullptr);

        GETSET_BINDABLE_ENUM_SETTING(TabSwitcherMode, Model::TabSwitcherMode, State().Globals, TabSwitcherMode);
        GETSET_BINDABLE_ENUM_SETTING(CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, State().Globals, CopyFormatting);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
