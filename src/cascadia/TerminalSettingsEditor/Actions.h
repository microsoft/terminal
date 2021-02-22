// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
#include "ActionsPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ActionsPageNavigationState : ActionsPageNavigationStateT<ActionsPageNavigationState>
    {
    public:
        ActionsPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
    };

    struct Actions : ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> FilteredActions();

        GETSET_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);

    private:
        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> _filteredActions{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
