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
    static inline void DismissAllPopups(const winrt::Windows::UI::Xaml::XamlRoot& xamlRoot)
    {
        const auto popups{ winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(xamlRoot) };
        for (const auto& p : popups)
        {
            p.IsOpen(false);
        }
    }
    // When the ScrollViewer scrolls, dismiss any popups we might have.
    void Launch::ViewChanging(const winrt::Windows::Foundation::IInspectable& sender,
                              const winrt::Windows::UI::Xaml::Controls::ScrollViewerViewChangingEventArgs& /*e*/)
    {
        // Inside this struct, we can't get at the XamlRoot() that our subclass
        // implements. I mean, _we_ can, but when XAML does its code
        // generation, _XAML_ won't be able to figure it out.
        //
        // Fortunately for us, we don't need to! The sender is a UIElement, so
        // we can just get _their_ XamlRoot().
        if (const auto& uielem{ sender.try_as<winrt::Windows::UI::Xaml::UIElement>() })
        {
            DismissAllPopups(uielem.XamlRoot());
        }
    }

}
