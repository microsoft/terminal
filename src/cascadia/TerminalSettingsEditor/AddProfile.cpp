// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AddProfile.h"
#include "AddProfile.g.cpp"
#include "AddProfilePageNavigationState.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AddProfile::AddProfile()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"AddProfile_AddNewTextBlock/Text"));
        Automation::AutomationProperties::SetName(DuplicateButton(), RS_(L"AddProfile_DuplicateTextBlock/Text"));
    }

    void AddProfile::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::AddProfilePageNavigationState>();
    }

    void AddProfile::AddNewClick(const IInspectable& /*sender*/,
                                 const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        _State.RequestAddNew();
    }

    void AddProfile::DuplicateClick(const IInspectable& /*sender*/,
                                    const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        if (const auto selected = Profiles().SelectedItem())
        {
            _State.RequestDuplicate(selected.try_as<Model::Profile>().Guid());
        }
    }

    void AddProfile::ProfilesSelectionChanged(const IInspectable& /*sender*/,
                                              const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        if (!_IsProfileSelected)
        {
            IsProfileSelected(true);
        }
    }
}
