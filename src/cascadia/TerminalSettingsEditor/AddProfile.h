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
            _AddNewHandlers(GUID{});
        }

        void RequestDuplicate(GUID profile)
        {
            _AddNewHandlers(profile);
        }

        GETSET_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        WINRT_CALLBACK(AddNew, AddNewArgs);
    };

    struct AddProfile : AddProfileT<AddProfile>
    {
    public:
        AddProfile();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void AddNewClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void DuplicateClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        GETSET_PROPERTY(Editor::AddProfilePageNavigationState, State, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
