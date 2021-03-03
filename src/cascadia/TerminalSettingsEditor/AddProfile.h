// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AddProfile.g.h"
#include "AddProfilePageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct AddProfilePageNavigationState : AddProfilePageNavigationStateT<AddProfilePageNavigationState>
    {
    public:
        AddProfilePageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        void RequestOpenJson(const Model::SettingsTarget target)
        {
            _OpenJsonHandlers(nullptr, target);
        }

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        TYPED_EVENT(OpenJson, Windows::Foundation::IInspectable, Model::SettingsTarget);
    };

    struct AddProfile : AddProfileT<AddProfile>
    {
    public:
        AddProfile();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> FilteredActions();

        GETSET_PROPERTY(Editor::AddProfilePageNavigationState, State, nullptr);

    private:
        friend struct AddProfileT<AddProfile>; // for Xaml to bind events
        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Command> _filteredActions{ nullptr };

        void _OpenSettingsClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
