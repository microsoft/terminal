// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include "MainPage.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch()
    {
        InitializeComponent();

        // find default profile index and initialize dropdown to that Profile
        const auto profiles{ MainPage::Settings().AllProfiles() };
        const auto defaultProfileGuid{ GlobalSettings().DefaultProfile() };
        for (uint32_t i = 0; i < profiles.Size(); ++i)
        {
            auto profileGuid{ profiles.GetAt(i).Guid() };
            if (profileGuid == defaultProfileGuid)
            {
                DefaultProfile().SelectedIndex(i);
            }
        }
    }

    GlobalAppSettings Launch::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }

    void Launch::DefaultProfileSelectionChanged(IInspectable const& sender,
                                                SelectionChangedEventArgs const& /*args*/)
    {
        const auto control{ winrt::unbox_value<ComboBox>(sender) };
        const auto selectedIndex{ control.SelectedIndex() };
        const auto selectedProfile{ MainPage::Settings().AllProfiles().GetAt(selectedIndex) };
        const auto profileGuid{ selectedProfile.Guid() };

        // set the default profile
        MainPage::Settings().GlobalSettings().DefaultProfile(profileGuid);
    }
}
