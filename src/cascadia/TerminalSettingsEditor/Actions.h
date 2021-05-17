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

        void RequestOpenJson(const Model::SettingsTarget target)
        {
            _OpenJsonHandlers(nullptr, target);
        }

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        TYPED_EVENT(OpenJson, Windows::Foundation::IInspectable, Model::SettingsTarget);
    };

    struct Actions : ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> FilteredActions();

        WINRT_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);

    private:
        friend struct ActionsT<Actions>; // for Xaml to bind events
        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> _filteredActions{ nullptr };

        void _OpenSettingsClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
