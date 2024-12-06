// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Advanced.h"
#include "Profiles_Advanced.g.cpp"
#include "ProfileViewModel.h"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Advanced::Profiles_Advanced()
    {
        InitializeComponent();
        // The XAML -> C++ converter seems to use floats instead of doubles,
        // which means it can't represent such a large number.
        // As such we have to set the property manually.
        HistorySizeBox().Maximum(1073741823.0);
    }

    void Profiles_Advanced::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _Profile = e.Parameter().as<Editor::ProfileViewModel>();
    }

    void Profiles_Advanced::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }
}
