// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemes.h"
#include "ColorTableEntry.g.cpp"
#include "ColorSchemes.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemes::ColorSchemes()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(ColorSchemeComboBox(), RS_(L"ColorScheme_Name/Header"));
        Automation::AutomationProperties::SetFullDescription(ColorSchemeComboBox(), RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        ToolTipService::SetToolTip(ColorSchemeComboBox(), box_value(RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip")));

        Automation::AutomationProperties::SetName(RenameButton(), RS_(L"Rename/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));

        Automation::AutomationProperties::SetName(NameBox(), RS_(L"ColorScheme_Name/Header"));
        Automation::AutomationProperties::SetFullDescription(NameBox(), RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        ToolTipService::SetToolTip(NameBox(), box_value(RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip")));

        Automation::AutomationProperties::SetName(RenameAcceptButton(), RS_(L"RenameAccept/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(RenameCancelButton(), RS_(L"RenameCancel/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"ColorScheme_AddNewButton/Text"));
        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"ColorScheme_DeleteButton/Text"));
    }

    void ColorSchemes::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ColorSchemesPageViewModel>();
    }

    void ColorSchemes::DeleteConfirmation_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.RequestDeleteCurrentScheme();
        DeleteButton().Flyout().Hide();

        // GH#11971, part 2. If we delete a scheme, and the next scheme we've
        // loaded is an inbox one that _can't_ be deleted, then we need to toss
        // focus to something sensible, rather than letting it fall out to the
        // tab item.
        //
        // When deleting a scheme and the next scheme _is_ deletable, this isn't
        // an issue, we'll already correctly focus the Delete button.
        //
        // However, it seems even more useful for focus to ALWAYS land on the
        // scheme dropdown box. This forces Narrator to read the name of the
        // newly selected color scheme, which seemed more useful.
        ColorSchemeComboBox().Focus(FocusState::Programmatic);
    }

    // Function Description:
    // - Pre-populates/focuses the name TextBox, updates the UI
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void ColorSchemes::Rename_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        NameBox().Text(_ViewModel.CurrentScheme().Name());
        _ViewModel.RequestEnterRename();
        NameBox().Focus(FocusState::Programmatic);
        NameBox().SelectAll();
    }

    void ColorSchemes::RenameAccept_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _RenameCurrentScheme(NameBox().Text());
        RenameButton().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::RenameCancel_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.RequestExitRename(false, {});
        RenameErrorTip().IsOpen(false);
        RenameButton().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::NameBox_PreviewKeyDown(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            _RenameCurrentScheme(NameBox().Text());
            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            RenameErrorTip().IsOpen(false);
            e.Handled(true);
        }
        ColorSchemeComboBox().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::_RenameCurrentScheme(hstring newName)
    {
        if (_ViewModel.RequestExitRename(true, newName))
        {
            // update the UI
            RenameErrorTip().IsOpen(false);

            // The color scheme is renamed appropriately, but the ComboBox still shows the old name (until you open it)
            // We need to manually force the ComboBox to refresh itself.
            const auto selectedIndex{ ColorSchemeComboBox().SelectedIndex() };
            ColorSchemeComboBox().SelectedIndex((selectedIndex + 1) % ViewModel().AllColorSchemes().Size());
            ColorSchemeComboBox().SelectedIndex(selectedIndex);
        }
        else
        {
            RenameErrorTip().Target(NameBox());
            RenameErrorTip().IsOpen(true);

            // focus the name box
            NameBox().Focus(FocusState::Programmatic);
            NameBox().SelectAll();
        }
    }
}
