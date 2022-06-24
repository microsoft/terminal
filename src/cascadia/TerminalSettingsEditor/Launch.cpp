// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch()
    {
        InitializeComponent();

        // BODGY
        // Xaml code generator for x:Bind to this will fail to find UnloadObject() on Launch class.
        // To work around, check it ourselves on construction and FindName to force load.
        // It's specified as x:Load=false in the XAML. So it only loads if this passes.
        if (CascadiaSettings::IsDefaultTerminalAvailable())
        {
            FindName(L"DefaultTerminalDropdown");
        }
    }

    void Launch::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::LaunchViewModel>();
    }
}
