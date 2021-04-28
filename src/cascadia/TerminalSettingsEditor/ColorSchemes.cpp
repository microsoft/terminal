// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemes.h"
#include "ColorTableEntry.g.cpp"
#include "ColorSchemes.g.cpp"
#include "ColorSchemesPageNavigationState.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // The first 8 entries of the color table are non-bright colors, whereas the rest are bright.
    static constexpr uint8_t ColorTableDivider{ 8 };

    static constexpr std::wstring_view ForegroundColorTag{ L"Foreground" };
    static constexpr std::wstring_view BackgroundColorTag{ L"Background" };
    static constexpr std::wstring_view CursorColorTag{ L"CursorColor" };
    static constexpr std::wstring_view SelectionBackgroundColorTag{ L"SelectionBackground" };

    static const std::array<hstring, 16> TableColorNames = {
        RS_(L"ColorScheme_Black/Header"),
        RS_(L"ColorScheme_Red/Header"),
        RS_(L"ColorScheme_Green/Header"),
        RS_(L"ColorScheme_Yellow/Header"),
        RS_(L"ColorScheme_Blue/Header"),
        RS_(L"ColorScheme_Purple/Header"),
        RS_(L"ColorScheme_Cyan/Header"),
        RS_(L"ColorScheme_White/Header"),
        RS_(L"ColorScheme_BrightBlack/Header"),
        RS_(L"ColorScheme_BrightRed/Header"),
        RS_(L"ColorScheme_BrightGreen/Header"),
        RS_(L"ColorScheme_BrightYellow/Header"),
        RS_(L"ColorScheme_BrightBlue/Header"),
        RS_(L"ColorScheme_BrightPurple/Header"),
        RS_(L"ColorScheme_BrightCyan/Header"),
        RS_(L"ColorScheme_BrightWhite/Header")
    };

    static const std::array<std::wstring, 9> InBoxSchemes = {
        L"Campbell",
        L"Campbell Powershell",
        L"Vintage",
        L"One Half Dark",
        L"One Half Light",
        L"Solarized Dark",
        L"Solarized Light",
        L"Tango Dark",
        L"Tango Light"
    };

    ColorSchemes::ColorSchemes() :
        _ColorSchemeList{ single_threaded_observable_vector<Model::ColorScheme>() },
        _CurrentNonBrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _CurrentBrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() }
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
        _State = e.Parameter().as<Editor::ColorSchemesPageNavigationState>();
        _UpdateColorSchemeList();

        // Initialize our color table view model with 16 dummy colors
        // so that on a ColorScheme selection change, we can loop through
        // each ColorTableEntry and just change its color. Performing a
        // clear and 16 appends doesn't seem to update the color pickers
        // very accurately.
        for (uint8_t i = 0; i < TableColorNames.size(); ++i)
        {
            const auto& entry{ winrt::make<ColorTableEntry>(i, Windows::UI::Color{ 0, 0, 0, 0 }) };
            if (i < ColorTableDivider)
            {
                _CurrentNonBrightColorTable.Append(entry);
            }
            else
            {
                _CurrentBrightColorTable.Append(entry);
            }
        }
        _CurrentForegroundColor = winrt::make<ColorTableEntry>(ForegroundColorTag, Windows::UI::Color{ 0, 0, 0, 0 });
        _CurrentBackgroundColor = winrt::make<ColorTableEntry>(BackgroundColorTag, Windows::UI::Color{ 0, 0, 0, 0 });
        _CurrentCursorColor = winrt::make<ColorTableEntry>(CursorColorTag, Windows::UI::Color{ 0, 0, 0, 0 });
        _CurrentSelectionBackgroundColor = winrt::make<ColorTableEntry>(SelectionBackgroundColorTag, Windows::UI::Color{ 0, 0, 0, 0 });

        // Try to look up the scheme that was navigated to. If we find it, immediately select it.
        const std::wstring lastNameFromNav{ _State.LastSelectedScheme() };
        const auto it = std::find_if(begin(_ColorSchemeList),
                                     end(_ColorSchemeList),
                                     [&lastNameFromNav](const auto& scheme) { return scheme.Name() == lastNameFromNav; });

        if (it != end(_ColorSchemeList))
        {
            auto scheme = *it;
            ColorSchemeComboBox().SelectedItem(scheme);
        }

        // populate color table grid
        const auto colorLabelStyle{ Resources().Lookup(winrt::box_value(L"ColorLabelStyle")).as<Windows::UI::Xaml::Style>() };
        const auto colorControlStyle{ Resources().Lookup(winrt::box_value(L"ColorControlStyle")).as<Windows::UI::Xaml::Style>() };
        const auto colorTableEntryTemplate{ Resources().Lookup(winrt::box_value(L"ColorTableEntryTemplate")).as<DataTemplate>() };
        auto setupColorControl = [colorTableEntryTemplate, colorControlStyle, colorTableGrid{ ColorTableGrid() }](const auto&& colorRef, const uint32_t& row, const uint32_t& col) {
            ContentControl colorControl{};
            colorControl.ContentTemplate(colorTableEntryTemplate);
            colorControl.Style(colorControlStyle);

            Data::Binding binding{};
            binding.Source(colorRef);
            binding.Mode(Data::BindingMode::TwoWay);
            colorControl.SetBinding(ContentControl::ContentProperty(), binding);

            colorTableGrid.Children().Append(colorControl);
            Grid::SetRow(colorControl, row);
            Grid::SetColumn(colorControl, col);
        };
        for (uint32_t row = 0; row < ColorTableGrid().RowDefinitions().Size(); ++row)
        {
            // color label
            TextBlock label{};
            label.Text(TableColorNames[row]);
            label.Style(colorLabelStyle);
            ColorTableGrid().Children().Append(label);
            Grid::SetRow(label, row);
            Grid::SetColumn(label, 0);

            // regular color
            setupColorControl(_CurrentNonBrightColorTable.GetAt(row), row, 1);

            // bright color
            setupColorControl(_CurrentBrightColorTable.GetAt(row), row, 2);
        }
    }

    // Function Description:
    // - Called when a different color scheme is selected. Updates our current
    //   color scheme and updates our currently modifiable color table.
    // Arguments:
    // - args: The selection changed args that tells us what's the new color scheme selected.
    // Return Value:
    // - <none>
    void ColorSchemes::ColorSchemeSelectionChanged(IInspectable const& /*sender*/,
                                                   SelectionChangedEventArgs const& args)
    {
        //  Update the color scheme this page is modifying
        const auto colorScheme{ args.AddedItems().GetAt(0).try_as<Model::ColorScheme>() };
        CurrentColorScheme(colorScheme);
        _UpdateColorTable(colorScheme);

        _State.LastSelectedScheme(colorScheme.Name());

        // Set the text disclaimer for the text box
        hstring disclaimer{};
        const std::wstring schemeName{ colorScheme.Name() };
        if (std::find(std::begin(InBoxSchemes), std::end(InBoxSchemes), schemeName) != std::end(InBoxSchemes))
        {
            // load disclaimer for in-box profiles
            disclaimer = RS_(L"ColorScheme_DeleteButtonDisclaimerInBox");
        }
        DeleteButtonDisclaimer().Text(disclaimer);

        // Update the state of the page
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CanDeleteCurrentScheme" });
        IsRenaming(false);
    }

    // Function Description:
    // - Updates the list of all color schemes available to choose from.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorSchemes::_UpdateColorSchemeList()
    {
        // Surprisingly, though this is called every time we navigate to the page,
        // the list does not keep growing on each navigation.
        const auto& colorSchemeMap{ _State.Settings().GlobalSettings().ColorSchemes() };
        for (const auto& pair : colorSchemeMap)
        {
            _ColorSchemeList.Append(pair.Value());
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
    void ColorSchemes::ColorPickerChanged(IInspectable const& sender,
                                          ColorChangedEventArgs const& args)
    {
        const til::color newColor{ args.NewColor() };
        if (const auto& picker{ sender.try_as<ColorPicker>() })
        {
            if (const auto& tag{ picker.Tag() })
            {
                if (const auto index{ tag.try_as<uint8_t>() })
                {
                    CurrentColorScheme().SetColorTableEntry(*index, newColor);
                    if (index < ColorTableDivider)
                    {
                        _CurrentNonBrightColorTable.GetAt(*index).Color(newColor);
                    }
                    else
                    {
                        _CurrentBrightColorTable.GetAt(*index - ColorTableDivider).Color(newColor);
                    }
                }
                else if (const auto stringTag{ tag.try_as<hstring>() })
                {
                    if (stringTag == ForegroundColorTag)
                    {
                        CurrentColorScheme().Foreground(newColor);
                        _CurrentForegroundColor.Color(newColor);
                    }
                    else if (stringTag == BackgroundColorTag)
                    {
                        CurrentColorScheme().Background(newColor);
                        _CurrentBackgroundColor.Color(newColor);
                    }
                    else if (stringTag == CursorColorTag)
                    {
                        CurrentColorScheme().CursorColor(newColor);
                        _CurrentCursorColor.Color(newColor);
                    }
                    else if (stringTag == SelectionBackgroundColorTag)
                    {
                        CurrentColorScheme().SelectionBackground(newColor);
                        _CurrentSelectionBackgroundColor.Color(newColor);
                    }
                }
            }
        }
    }

    bool ColorSchemes::CanDeleteCurrentScheme() const
    {
        if (const auto& scheme{ CurrentColorScheme() })
        {
            // Only allow this color scheme to be deleted if it's not provided in-box
            const std::wstring myName{ scheme.Name() };
            return std::find(std::begin(InBoxSchemes), std::end(InBoxSchemes), myName) == std::end(InBoxSchemes);
        }
        return false;
    }

    void ColorSchemes::DeleteConfirmation_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        const auto schemeName{ CurrentColorScheme().Name() };
        _State.Settings().GlobalSettings().RemoveColorScheme(schemeName);

        // This ensures that the JSON is updated with "Campbell", because the color scheme was deleted
        _State.Settings().UpdateColorSchemeReferences(schemeName, L"Campbell");

        const auto removedSchemeIndex{ ColorSchemeComboBox().SelectedIndex() };
        if (static_cast<uint32_t>(removedSchemeIndex) < _ColorSchemeList.Size() - 1)
        {
            // select same index
            ColorSchemeComboBox().SelectedIndex(removedSchemeIndex + 1);
        }
        else
        {
            // select last color scheme (avoid out of bounds error)
            ColorSchemeComboBox().SelectedIndex(removedSchemeIndex - 1);
        }
        _ColorSchemeList.RemoveAt(removedSchemeIndex);
        DeleteButton().Flyout().Hide();
    }

    void ColorSchemes::AddNew_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        // Give the new scheme a distinct name
        const hstring schemeName{ fmt::format(L"Color Scheme {}", _State.Settings().GlobalSettings().ColorSchemes().Size() + 1) };
        Model::ColorScheme scheme{ schemeName };

        // Add the new color scheme
        _State.Settings().GlobalSettings().AddColorScheme(scheme);

        // Update current page
        _ColorSchemeList.Append(scheme);
        ColorSchemeComboBox().SelectedItem(scheme);
    }

    // Function Description:
    // - Pre-populates/focuses the name TextBox, updates the UI
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void ColorSchemes::Rename_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        NameBox().Text(CurrentColorScheme().Name());
        IsRenaming(true);
        NameBox().Focus(FocusState::Programmatic);
        NameBox().SelectAll();
    }

    void ColorSchemes::RenameAccept_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _RenameCurrentScheme(NameBox().Text());
        RenameButton().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::RenameCancel_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        IsRenaming(false);
        RenameErrorTip().IsOpen(false);
        RenameButton().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::NameBox_PreviewKeyDown(IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
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
        // check if different name is already in use
        const auto oldName{ CurrentColorScheme().Name() };
        if (newName != oldName && _State.Settings().GlobalSettings().ColorSchemes().HasKey(newName))
        {
            // open the error tip
            RenameErrorTip().Target(NameBox());
            RenameErrorTip().IsOpen(true);

            // focus the name box
            NameBox().Focus(FocusState::Programmatic);
            NameBox().SelectAll();
            return;
        }

        // update the settings model
        CurrentColorScheme().Name(newName);
        _State.Settings().GlobalSettings().RemoveColorScheme(oldName);
        _State.Settings().GlobalSettings().AddColorScheme(CurrentColorScheme());
        _State.Settings().UpdateColorSchemeReferences(oldName, newName);

        // update the UI
        RenameErrorTip().IsOpen(false);
        CurrentColorScheme().Name(newName);
        IsRenaming(false);

        // The color scheme is renamed appropriately, but the ComboBox still shows the old name (until you open it)
        // We need to manually force the ComboBox to refresh itself.
        const auto selectedIndex{ ColorSchemeComboBox().SelectedIndex() };
        ColorSchemeComboBox().SelectedIndex((selectedIndex + 1) % ColorSchemeList().Size());
        ColorSchemeComboBox().SelectedIndex(selectedIndex);
    }

    // Function Description:
    // - Updates the currently modifiable color table based on the given current color scheme.
    // Arguments:
    // - colorScheme: the color scheme to retrieve the color table from
    // Return Value:
    // - <none>
    void ColorSchemes::_UpdateColorTable(const Model::ColorScheme& colorScheme)
    {
        for (uint8_t i = 0; i < TableColorNames.size(); ++i)
        {
            til::color currentColor{ colorScheme.Table()[i] };
            if (i < ColorTableDivider)
            {
                _CurrentNonBrightColorTable.GetAt(i).Color(currentColor);
            }
            else
            {
                _CurrentBrightColorTable.GetAt(i - ColorTableDivider).Color(currentColor);
            }
        }
        _CurrentForegroundColor.Color(til::color{ colorScheme.Foreground() });
        _CurrentBackgroundColor.Color(til::color{ colorScheme.Background() });
        _CurrentCursorColor.Color(til::color{ colorScheme.CursorColor() });
        _CurrentSelectionBackgroundColor.Color(til::color{ colorScheme.SelectionBackground() });
    }

    ColorTableEntry::ColorTableEntry(uint8_t index, Windows::UI::Color color)
    {
        Name(TableColorNames[index]);
        Tag(winrt::box_value<uint8_t>(index));
        Color(color);
    }

    ColorTableEntry::ColorTableEntry(std::wstring_view tag, Windows::UI::Color color)
    {
        Name(LocalizedNameForEnumName(L"ColorScheme_", tag, L"Text"));
        Tag(winrt::box_value(tag));
        Color(color);
    }
}
