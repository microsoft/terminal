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

        void RequestAddNew()
        {
            _AddNewHandlers(nullptr, nullptr);
        }

        void RequestDuplicate(GUID profile)
        {
            _AddNewHandlers(nullptr, winrt::box_value(profile));
        }

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        TYPED_EVENT(AddNew, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable);
    };

    struct AddProfile : AddProfileT<AddProfile>
    {
    public:
        AddProfile();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        GETSET_PROPERTY(Editor::AddProfilePageNavigationState, State, nullptr);

    private:
        friend struct AddProfileT<AddProfile>; // for Xaml to bind events

        void _AddNewClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _DuplicateClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
