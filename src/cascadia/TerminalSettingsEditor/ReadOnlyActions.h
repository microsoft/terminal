// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ReadOnlyActions.g.h"
#include "ReadOnlyActionsPageNavigationState.g.h"
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

    struct ReadOnlyActionsPageNavigationState : ReadOnlyActionsPageNavigationStateT<ReadOnlyActionsPageNavigationState>
    {
    public:
        ReadOnlyActionsPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        void RequestOpenJson(const Model::SettingsTarget target)
        {
            _OpenJsonHandlers(nullptr, target);
        }

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        TYPED_EVENT(OpenJson, Windows::Foundation::IInspectable, Model::SettingsTarget);
    };

    struct ReadOnlyActions : public HasScrollViewer<ReadOnlyActions>, ReadOnlyActionsT<ReadOnlyActions>
    {
    public:
        ReadOnlyActions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> FilteredActions();

        WINRT_PROPERTY(Editor::ReadOnlyActionsPageNavigationState, State, nullptr);

    private:
        friend struct ReadOnlyActionsT<ReadOnlyActions>; // for Xaml to bind events
        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> _filteredActions{ nullptr };

        void _OpenSettingsClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ReadOnlyActions);
}
