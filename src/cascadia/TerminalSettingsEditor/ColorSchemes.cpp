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

        // Initialize our list of color schemes and initially set color scheme and table.
        auto colorSchemeMap = MainPage::Settings().GlobalSettings().ColorSchemes();
        for (const auto& pair : MainPage::Settings().GlobalSettings().ColorSchemes())
        {
            _ColorSchemeList.Append(pair.Key());
        }
    }

    IObservableVector<hstring> ColorSchemes::ColorSchemeList()
    {
        return _ColorSchemeList;
    }

    void ColorSchemes::ColorSchemeSelectionChanged(IInspectable const& /*sender*/,
                                                   SelectionChangedEventArgs const& args)
    {
        //  Update the color scheme this page is modifying
        auto str = winrt::unbox_value<hstring>(args.AddedItems().GetAt(0));
        auto colorScheme = MainPage::Settings().GlobalSettings().ColorSchemes().Lookup(str);
        CurrentColorScheme(colorScheme);
        _UpdateColorTable(colorScheme);
    }

    void ColorSchemes::_UpdateColorSchemeList()
    {
        auto colorSchemeMap = MainPage::Settings().GlobalSettings().ColorSchemes();
        for (const auto& pair : MainPage::Settings().GlobalSettings().ColorSchemes())
        {
            _ColorSchemeList.Append(pair.Key());
        }
    }

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

    // Update the Page's displayed color table
    void ColorSchemes::_UpdateColorTable(const Model::ColorScheme& colorScheme)
    {
        _CurrentColorTable.Clear();
        for (uint8_t i = 0; i < TableColorNames.size(); ++i)
        {
            auto entry = winrt::make<ColorTableEntry>(i, colorScheme.Table()[i]);
            _CurrentColorTable.Append(entry);
        }
    }

    void ColorSchemes::BeforeTextChanging(IInspectable const& sender,
                                          TextBoxBeforeTextChangingEventArgs const& args)
    {
        // Some naive input checks to try to keep all text input within hexadecimal.
        if (auto textBox = sender.try_as<TextBox>())
        {
            std::wstring_view text{ textBox.Text() };
            std::wstring_view newText{ args.NewText() };

            if (newText.at(0) != '#')
            {
                args.Cancel(true);
                return;
            }
            
            for (auto c : newText.substr(1))
            {
                if (!std::iswalnum(c))
                {
                    args.Cancel(true);
                    return;
                }
            }

            if (newText.size() > 9)
            {
                args.Cancel(true);
            }

        }
    }

    void ColorSchemes::TextBoxGotFocus(IInspectable const& sender,
                                       RoutedEventArgs const& /*args*/)
    {
        if (auto textBox = sender.try_as<TextBox>())
        {
            // Save the text of the textbox being modified.
            _textBoxSavedText = textBox.Text();
        }
    }

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
                if (winrt::unbox_value_or<uint8_t>(tag, 17) != 17)
                {
                    auto index = winrt::unbox_value<uint8_t>(tag);
                    CurrentColorScheme().SetColorTableEntry(index, newColor);
                    CurrentColorTable().GetAt(index).Color(newColor);
                }
                else if (winrt::unbox_value_or<hstring>(tag, L"invalid") != L"invalid")
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
