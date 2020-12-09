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

    ColorSchemes::ColorSchemes() :
        _ColorSchemeList{ single_threaded_observable_vector<hstring>() },
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
        auto str = winrt::unbox_value<hstring>(args.AddedItems().GetAt(0));
        auto colorScheme = _State.Globals().ColorSchemes().Lookup(str);
        CurrentColorScheme(colorScheme);
        _UpdateColorTable(colorScheme);
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
        const auto& colorSchemeMap{ _State.Globals().ColorSchemes() };
        for (const auto& pair : colorSchemeMap)
        {
            _ColorSchemeList.Append(pair.Key());
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
