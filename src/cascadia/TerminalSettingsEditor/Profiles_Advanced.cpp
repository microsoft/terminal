// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Advanced.h"
#include "Profiles_Advanced.g.cpp"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Advanced::Profiles_Advanced()
    {
        InitializeComponent();
    }

    void Profiles_Advanced::OnNavigatedTo(const NavigationEventArgs& e)
    {
        auto state{ e.Parameter().as<Editor::ProfilePageNavigationState>() };
        _Profile = state.Profile();

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _Profile.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"AntialiasingMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            }
            else if (settingName == L"CloseOnExit")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            }
            else if (settingName == L"BellStyle")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsBellStyleFlagSet" });
            }
        });
    }

    void Profiles_Advanced::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }
}
