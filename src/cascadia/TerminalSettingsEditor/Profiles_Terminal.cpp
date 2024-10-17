// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Terminal.h"
#include "Profiles_Terminal.g.cpp"
#include "ProfileViewModel.h"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Terminal::Profiles_Terminal()
    {
        InitializeComponent();
    }

    void Profiles_Terminal::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _Profile = e.Parameter().as<Editor::ProfileViewModel>();
    }

    void Profiles_Terminal::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }
}
