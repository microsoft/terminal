// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "ColorSchemes.h"
#include "ColorSchemes.g.cpp"

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
    static constexpr std::array<std::string_view, 16> TableColors = {
        "Black",
        "Red",
        "Green",
        "Yellow",
        "Blue",
        "Purple",
        "Cyan",
        "White",
        "Bright Black",
        "Bright Red",
        "Bright Green",
        "Bright Yellow",
        "Bright Blue",
        "Bright Purple",
        "Bright Cyan",
        "Bright White"
    };

    static constexpr std::array<std::string_view, 4> OtherColors = {
        "Background",
        "Foreground",
        "Selection Background",
        "Cursor Color"
    };

    ColorSchemes::ColorSchemes():
        _ColorSchemeList{ single_threaded_observable_vector<hstring>() },
        _CurrentColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _CurrentOtherColors{ single_threaded_observable_vector<Editor::ColorTableEntry>() }
    {
        InitializeComponent();

        // Initialize our list of color schemes and initially set color scheme and table.
        auto colorSchemeMap = MainPage::Settings().GlobalSettings().ColorSchemes();
        for (const auto& pair : MainPage::Settings().GlobalSettings().ColorSchemes())
        {
            _ColorSchemeList.Append(pair.Key());
        }
    }

    // Converts Color to a Brush
    Windows::UI::Xaml::Media::Brush ColorTableEntry::ColorToBrush(Windows::UI::Color color)
    {
        return SolidColorBrush(color);
    }

    // Returns a map of all ColorScheme names to their color scheme
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

    void ColorSchemes::ColorPickerChanged(IInspectable const& sender,
                                          ColorChangedEventArgs const& args)
    {
        if (auto picker = sender.try_as<ColorPicker>())
        {
            auto tag = winrt::unbox_value<hstring>(picker.Tag());

            if (tag == L"Background")
            {
                CurrentColorScheme().Background(args.NewColor());
            }
            else if (tag == L"Foreground")
            {
                CurrentColorScheme().Foreground(args.NewColor());
            }
            else if (tag == L"Selection Background")
            {
                CurrentColorScheme().SelectionBackground(args.NewColor());
            }
            else if (tag == L"Cursor Color")
            {
                CurrentColorScheme().CursorColor(args.NewColor());
            }
        }
        if (auto test = sender.try_as<ColorTableEntry>())
        {

        }
    }

    void ColorSchemes::_UpdateColorSchemeList()
    {
        auto colorSchemeMap = MainPage::Settings().GlobalSettings().ColorSchemes();
        for (const auto& pair : MainPage::Settings().GlobalSettings().ColorSchemes())
        {
            _ColorSchemeList.Append(pair.Key());
        }
    }

    // Update the Page's displayed color table
    void ColorSchemes::_UpdateColorTable(const Model::ColorScheme& colorScheme)
    {
        _CurrentColorTable.Clear();
        for (uint32_t i = 0; i < TableColors.size(); ++i)
        {
            auto entry = winrt::make<ColorTableEntry>(to_hstring(TableColors[i]), colorScheme.Table()[i]);
            _CurrentColorTable.Append(entry);
        }

        _CurrentOtherColors.Clear();
        _CurrentOtherColors.Append(winrt::make<ColorTableEntry>(L"Background", colorScheme.Background()));
        _CurrentOtherColors.Append(winrt::make<ColorTableEntry>(L"Foreground", colorScheme.Foreground()));
        _CurrentOtherColors.Append(winrt::make<ColorTableEntry>(L"Selection Background", colorScheme.SelectionBackground()));
        _CurrentOtherColors.Append(winrt::make<ColorTableEntry>(L"Cursor Color", colorScheme.CursorColor()));
    }

    ColorTableEntry::ColorTableEntry(winrt::hstring name, Windows::UI::Color color)
    {
        Name(name);
        Color(color);
    }
}
