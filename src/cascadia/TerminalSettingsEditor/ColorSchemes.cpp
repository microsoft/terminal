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
        _CurrentColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() }
    {
        InitializeComponent();
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
            auto entry = winrt::make<ColorTableEntry>(i, Windows::UI::Color{ 0, 0, 0, 0 });
            _CurrentColorTable.Append(entry);
        }

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
        if (auto picker = sender.try_as<ColorPicker>())
        {
            if (auto tag = picker.Tag())
            {
                auto index = winrt::unbox_value<uint8_t>(tag);
                CurrentColorScheme().SetColorTableEntry(index, args.NewColor());
                _CurrentColorTable.GetAt(index).Color(args.NewColor());
            }
        }
    }

    bool ColorSchemes::CanDeleteCurrentScheme() const
    {
        if (const auto scheme{ CurrentColorScheme() })
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
    }

    void ColorSchemes::RenameCancel_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        IsRenaming(false);
        RenameErrorTip().IsOpen(false);
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
            _CurrentColorTable.GetAt(i).Color(colorScheme.Table()[i]);
        }
    }

    ColorTableEntry::ColorTableEntry(uint8_t index, Windows::UI::Color color)
    {
        Name(TableColorNames[index]);
        Index(winrt::box_value<uint8_t>(index));
        Color(color);
    }
}
