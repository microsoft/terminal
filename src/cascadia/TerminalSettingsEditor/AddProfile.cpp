// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AddProfile.h"
#include "AddProfile.g.cpp"
#include "AddProfilePageNavigationState.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AddProfile::AddProfile()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Model::Command>();
    }

    void AddProfile::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::AddProfilePageNavigationState>();

        for (const auto& [k, command] : _State.Settings().GlobalSettings().Commands())
        {
            // Filter out nested commands, and commands that aren't bound to a
            // key. This page is currently just for displaying the actions that
            // _are_ bound to keys.
            if (command.HasNestedCommands() || command.KeyChordText().empty())
            {
                continue;
            }
            _filteredActions.Append(command);
        }
    }

    Collections::IObservableVector<Command> AddProfile::FilteredActions()
    {
        return _filteredActions;
    }

    void AddProfile::_OpenSettingsClick(const IInspectable& /*sender*/,
                                        const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;

        _State.RequestOpenJson(target);
    }

}
