// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
#include "ActionsPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct CommandComparator
    {
        bool operator()(const Model::Command& lhs, const Model::Command& rhs) const
        {
            return lhs.Name() < rhs.Name();
        }
    };

    struct ActionsPageNavigationState : ActionsPageNavigationStateT<ActionsPageNavigationState>
    {
    public:
        ActionsPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
    };

    struct Actions : ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
