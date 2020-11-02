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
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch() :
        _profileNameList{ winrt::single_threaded_observable_vector<hstring>() }
    {
        InitializeComponent();

        // update list of profile names for DefaultProfile
        // find default profile index and initialize dropdown to that Profile
        _UpdateProfileNameList(MainPage::Settings().AllProfiles());
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
        // Deduce what the profile is
        //auto profileName{ winrt::unbox_value<hstring>(args.AddedItems().GetAt(0)) };
        //NewTerminalArgs terminalArgs{};
        //terminalArgs.Profile(profileName);
        //auto profileGuid{ MainPage::Settings().GetProfileForArgs(terminalArgs) };

        auto control{ winrt::unbox_value<ComboBox>(sender) };
        auto selectedIndex{ control.SelectedIndex() };
        auto selectedProfile{ MainPage::Settings().AllProfiles().GetAt(selectedIndex) };
        auto profileGuid{ selectedProfile.Guid() };

        // set the default profile
        MainPage::Settings().GlobalSettings().DefaultProfile(profileGuid);
    }

    Windows::Foundation::Collections::IObservableVector<hstring> Launch::ProfileNameList()
    {
        return _profileNameList;
    }

    // Function Description:
    // - Updates the list of all profile names available to choose from.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Launch::_UpdateProfileNameList(IObservableVector<Profile> profiles)
    {
        _profileNameList.Clear();
        for (const auto& profile : profiles)
        {
            _profileNameList.Append(profile.Name());
        }
    }
}
