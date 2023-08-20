// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Xaml;
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

        Automation::AutomationProperties::SetName(LaunchModeComboBox(), RS_(L"Globals_LaunchModeSetting/Text"));
        Automation::AutomationProperties::SetHelpText(LaunchModeComboBox(), RS_(L"Globals_LaunchModeSetting/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(PosXBox(), RS_(L"Globals_InitialPosXBox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(PosYBox(), RS_(L"Globals_InitialPosYBox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetHelpText(UseDefaultLaunchPositionCheckbox(), RS_(L"Globals_DefaultLaunchPositionCheckbox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(CenterOnLaunchToggle(), RS_(L"Globals_CenterOnLaunch/Text"));
        Automation::AutomationProperties::SetHelpText(CenterOnLaunchToggle(), RS_(L"Globals_CenterOnLaunch/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
    }

    void Launch::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::LaunchViewModel>();
    }
}
