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
    }

    void Profiles_Advanced::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }
}
