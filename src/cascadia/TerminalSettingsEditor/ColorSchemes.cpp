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

        ColorSchemeComboBox().SelectedItem(_ViewModel.CurrentScheme());
    }

    // Function Description:
    // - Called when a different color scheme is selected. Updates our current
    //   color scheme and updates our currently modifiable color table.
    // Arguments:
    // - args: The selection changed args that tells us what's the new color scheme selected.
    // Return Value:
    // - <none>
    void ColorSchemes::ColorSchemeSelectionChanged(const IInspectable& /*sender*/,
                                                   const SelectionChangedEventArgs& args)
    {
        //  Update the color scheme this page is modifying
        if (args.AddedItems().Size() > 0)
        {
            const auto colorScheme{ args.AddedItems().GetAt(0).try_as<ColorSchemeViewModel>() };
            _ViewModel.RequestSetCurrentScheme(*colorScheme);

            // Set the text disclaimer for the text box
            hstring disclaimer{};
            if (!_ViewModel.CanDeleteCurrentScheme())
            {
                // load disclaimer for in-box profiles
                disclaimer = RS_(L"ColorScheme_DeleteButtonDisclaimerInBox");
            }
            DeleteButtonDisclaimer().Text(disclaimer);

            IsRenaming(false);
        }
    }

    // Function Description:
    // - Called when a ColorPicker control has selected a new color. This is specifically
    //   called by color pickers assigned to a color table entry. It takes the index
    //   that's been stuffed in the Tag property of the color picker and uses it
    //   to update the color table accordingly.
    // Arguments:
    // - sender: the color picker that raised this event.
    // - args: the args that contains the new color that was picked.
    // Return Value:
    // - <none>
    void ColorSchemes::ColorPickerChanged(const IInspectable& sender,
                                          const MUX::Controls::ColorChangedEventArgs& args)
    {
        const til::color newColor{ args.NewColor() };
        if (const auto& picker{ sender.try_as<MUX::Controls::ColorPicker>() })
        {
            if (const auto& tag{ picker.Tag() })
            {
                if (const auto index{ tag.try_as<uint8_t>() })
                {
                    if (index < ColorTableDivider)
                    {
                        _ViewModel.CurrentScheme().NonBrightColorTable().GetAt(*index).Color(newColor);
                    }
                    else
                    {
                        _ViewModel.CurrentScheme().BrightColorTable().GetAt(*index - ColorTableDivider).Color(newColor);
                    }
                }
                else if (const auto stringTag{ tag.try_as<hstring>() })
                {
                    if (stringTag == ForegroundColorTag)
                    {
                        _ViewModel.CurrentScheme().ForegroundColor().Color(newColor);
                    }
                    else if (stringTag == BackgroundColorTag)
                    {
                        _ViewModel.CurrentScheme().BackgroundColor().Color(newColor);
                    }
                    else if (stringTag == CursorColorTag)
                    {
                        _ViewModel.CurrentScheme().CursorColor().Color(newColor);
                    }
                    else if (stringTag == SelectionBackgroundColorTag)
                    {
                        _ViewModel.CurrentScheme().SelectionBackgroundColor().Color(newColor);
                    }
                }
            }
        }
    }

    void ColorSchemes::DeleteConfirmation_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        const auto removedSchemeIndex{ ColorSchemeComboBox().SelectedIndex() };
        _ViewModel.RequestDeleteCurrentScheme();
        if (static_cast<uint32_t>(removedSchemeIndex) < ViewModel().AllColorSchemes().Size())
        {
            // select same index
            ColorSchemeComboBox().SelectedIndex(removedSchemeIndex);
        }
        else
        {
            // select last color scheme (avoid out of bounds error)
            ColorSchemeComboBox().SelectedIndex(removedSchemeIndex - 1);
        }
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

    void ColorSchemes::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // Update current page
        ColorSchemeComboBox().SelectedItem(_ViewModel.RequestAddNew());
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
            IsRenaming(false);
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
            IsRenaming(false);

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
