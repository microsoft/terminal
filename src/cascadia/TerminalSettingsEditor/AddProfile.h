/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AddProfile.h

Abstract:
- This creates the 'add new profile' page in the settings UI and handles
  user interaction with it, raising events to the main page as necessary


Author(s):
- Pankaj Bhojwani - March 2021

--*/

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
            _AddNewHandlers(winrt::guid{});
        }

        void RequestDuplicate(GUID profile)
        {
            _AddNewHandlers(profile);
        }

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
        WINRT_CALLBACK(AddNew, AddNewArgs);
    };

    struct AddProfile : AddProfileT<AddProfile>
    {
    public:
        AddProfile();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void AddNewClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void DuplicateClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        WINRT_PROPERTY(Editor::AddProfilePageNavigationState, State, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AddProfile);
}
