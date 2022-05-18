// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemeViewModel.h"
#include "ColorSchemeViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static constexpr uint8_t ColorTableDivider{ 8 };
    static constexpr uint8_t ColorTableSize{ 16 };

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

    ColorSchemeViewModel::ColorSchemeViewModel(const Model::ColorScheme scheme) :
        _scheme{ scheme },
        _CurrentNonBrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _CurrentBrightColorTable { single_threaded_observable_vector<Editor::ColorTableEntry>() }
    {
        _Name = scheme.Name();

        const auto colorEntryChangedHandler = [&](const IInspectable& sender, const PropertyChangedEventArgs& args) {
            if (const auto entry{ sender.try_as<ColorTableEntry>() })
            {
                if (args.PropertyName() == L"Color")
                {
                    const til::color newColor{ entry->Color() };
                    if (const auto& tag{ entry->Tag() })
                    {
                        if (const auto index{ tag.try_as<uint8_t>() })
                        {
                            _scheme.SetColorTableEntry(*index, newColor);
                        }
                        else if (const auto stringTag{ tag.try_as<hstring>() })
                        {
                            if (stringTag == ForegroundColorTag)
                            {
                                _scheme.Foreground(newColor);
                            }
                            else if (stringTag == BackgroundColorTag)
                            {
                                _scheme.Background(newColor);
                            }
                            else if (stringTag == CursorColorTag)
                            {
                                _scheme.CursorColor(newColor);
                            }
                            else if (stringTag == SelectionBackgroundColorTag)
                            {
                                _scheme.SelectionBackground(newColor);
                            }
                        }
                    }
                }
            }
        };

        for (uint8_t i = 0; i < ColorTableSize; ++i)
        {
            til::color currentColor{ scheme.Table()[i] };
            const auto& entry{ winrt::make<ColorTableEntry>(i, currentColor) };
            entry.PropertyChanged(colorEntryChangedHandler);
            if (i < ColorTableDivider)
            {
                _CurrentNonBrightColorTable.Append(entry);
            }
            else
            {
                _CurrentBrightColorTable.Append(entry);
            }
        }

        _CurrentForegroundColor = winrt::make<ColorTableEntry>(ForegroundColorTag, til::color(scheme.Foreground()));
        _CurrentBackgroundColor = winrt::make<ColorTableEntry>(BackgroundColorTag, til::color(scheme.Background()));
        _CurrentCursorColor = winrt::make<ColorTableEntry>(CursorColorTag, til::color(scheme.CursorColor()));
        _CurrentSelectionBackgroundColor = winrt::make<ColorTableEntry>(SelectionBackgroundColorTag, til::color(scheme.SelectionBackground()));

        _CurrentForegroundColor.PropertyChanged(colorEntryChangedHandler);
        _CurrentBackgroundColor.PropertyChanged(colorEntryChangedHandler);
        _CurrentCursorColor.PropertyChanged(colorEntryChangedHandler);
        _CurrentSelectionBackgroundColor.PropertyChanged(colorEntryChangedHandler);
    }

    Editor::ColorTableEntry ColorSchemeViewModel::ColorEntryAt(uint8_t index)
    {
        if (index < ColorTableDivider)
        {
            return _CurrentNonBrightColorTable.GetAt(index);
        }
        else
        {
            return _CurrentBrightColorTable.GetAt(index - ColorTableDivider);
        }
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

    Windows::UI::Color ColorTableEntry::Color()
    {
        return _color;
    }

    void ColorTableEntry::Color(Windows::UI::Color newColor)
    {
        _color = newColor;
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Color" });
    }
}
