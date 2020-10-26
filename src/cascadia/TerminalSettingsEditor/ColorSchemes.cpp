// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "ColorSchemes.h"
#include "ColorTableEntry.g.cpp"
#include "ColorSchemes.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

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
        _UpdateColorSchemeList();
    }

    IObservableVector<hstring> ColorSchemes::ColorSchemeList()
    {
        return _ColorSchemeList;
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
        auto colorScheme = MainPage::Settings().GlobalSettings().ColorSchemes().Lookup(str);
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
        auto colorSchemeMap = MainPage::Settings().GlobalSettings().ColorSchemes();
        for (const auto& pair : MainPage::Settings().GlobalSettings().ColorSchemes())
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
        _CurrentColorTable.Clear();
        for (uint8_t i = 0; i < TableColorNames.size(); ++i)
        {
            auto entry = winrt::make<ColorTableEntry>(i, colorScheme.Table()[i]);
            _CurrentColorTable.Append(entry);
        }
    }

    // Function Description:
    // - Called when a TextBox receives focus. We use this to save
    //   the text before any changes are made in case an invalid
    //   hex code is entered in a TextBox.
    // Arguments:
    // - sender: the textbox that raised the event.
    // Return Value:
    // - <none>
    void ColorSchemes::TextBoxGotFocus(IInspectable const& sender,
                                       RoutedEventArgs const& /*args*/)
    {
        if (auto textBox = sender.try_as<TextBox>())
        {
            // Save the text of the textbox being modified.
            _textBoxSavedText = textBox.Text();
        }
    }

    // Function Description:
    // - Called when a TextBox loses focus. We use this handler along with
    //   a OneWay binding to the color because it gives us the ability to revert
    //   the text in a TextBox back to the original hex color if an invalid
    //   hex color is given.
    //   Since this handler is used for both color table TextBoxes and non color table
    //   boxes, we use the Tag property of the TextBox to identify what color to update.
    //   If the Tag is an integer, we need to update a color in the color table. If the Tag
    //   is a string, we need to update one of the Foreground, Background, etc, colors.
    // Arguments:
    // - sender: the textbox that just lost focus.
    // Return Value:
    // - <none>
    void ColorSchemes::TextBoxLostFocus(IInspectable const& sender,
                                        RoutedEventArgs const& /*args*/)
    {
        if (auto textBox = sender.try_as<TextBox>())
        {
            if (auto tag = textBox.Tag())
            {
                til::color newColor;
                try
                {
                    newColor = til::color::HexToColor(textBox.Text().c_str());
                }
                catch (...)
                {
                    // If the string wasn't a valid hex string, revert it back to the original color
                    textBox.Text(_textBoxSavedText);
                    return;
                }

                // The tag is either an index to our color table, or a string representing the
                // colors not in the color table (e.g. Foreground, Background).
                auto propertyValue = tag.as<IPropertyValue>();
                if (propertyValue.Type() == Windows::Foundation::PropertyType::UInt8)
                {
                    auto index = winrt::unbox_value<uint8_t>(tag);
                    CurrentColorScheme().SetColorTableEntry(index, newColor);
                    CurrentColorTable().GetAt(index).Color(newColor);
                }
                else if (propertyValue.Type() == Windows::Foundation::PropertyType::String)
                {
                    auto colorID = winrt::unbox_value<hstring>(tag);
                    if (colorID == L"Foreground")
                    {
                        CurrentColorScheme().Foreground(newColor);
                    }
                    else if (colorID == L"Background")
                    {
                        CurrentColorScheme().Background(newColor);
                    }
                    else if (colorID == L"CursorColor")
                    {
                        CurrentColorScheme().CursorColor(newColor);
                    }
                    else if (colorID == L"SelectionBackground")
                    {
                        CurrentColorScheme().SelectionBackground(newColor);
                    }
                }
            }
        }
    }

    Windows::UI::Xaml::Media::Brush ColorSchemes::ColorToBrush(Windows::UI::Color color)
    {
        return SolidColorBrush(color);
    }

    ColorTableEntry::ColorTableEntry(uint8_t index, Windows::UI::Color color)
    {
        Name(TableColorNames[index]);
        Index(winrt::box_value<uint8_t>(index));
        Color(color);
    }
}
