// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemeViewModel.h"
#include "ColorSchemeViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemeViewModel::ColorSchemeViewModel(const Model::ColorScheme scheme, const Editor::ColorSchemesPageViewModel parentPageVM) :
        _scheme{ scheme },
        _NonBrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _BrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _parentPageVM{ parentPageVM }
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
                _NonBrightColorTable.Append(entry);
            }
            else
            {
                _BrightColorTable.Append(entry);
            }
        }

        _ForegroundColor = winrt::make<ColorTableEntry>(ForegroundColorTag, til::color(scheme.Foreground()));
        _BackgroundColor = winrt::make<ColorTableEntry>(BackgroundColorTag, til::color(scheme.Background()));
        _CursorColor = winrt::make<ColorTableEntry>(CursorColorTag, til::color(scheme.CursorColor()));
        _SelectionBackgroundColor = winrt::make<ColorTableEntry>(SelectionBackgroundColorTag, til::color(scheme.SelectionBackground()));

        _ForegroundColor.PropertyChanged(colorEntryChangedHandler);
        _BackgroundColor.PropertyChanged(colorEntryChangedHandler);
        _CursorColor.PropertyChanged(colorEntryChangedHandler);
        _SelectionBackgroundColor.PropertyChanged(colorEntryChangedHandler);
    }

    winrt::hstring ColorSchemeViewModel::Name()
    {
        return _Name;
    }

    bool ColorSchemeViewModel::RequestRename(winrt::hstring newName)
    {
        return _parentPageVM.RequestRenameCurrentScheme(newName);
    }

    void ColorSchemeViewModel::Name(winrt::hstring newName)
    {
        _scheme.Name(newName);
        _Name = newName;
    }

    Editor::ColorTableEntry ColorSchemeViewModel::ColorEntryAt(uint32_t index)
    {
        if (index < ColorTableDivider)
        {
            return _NonBrightColorTable.GetAt(index);
        }
        else
        {
            return _BrightColorTable.GetAt(index - ColorTableDivider);
        }
    }

    ColorTableEntry::ColorTableEntry(uint8_t index, Windows::UI::Color color)
    {
        static const std::array<hstring, 16> TableColorNames = {
            RS_(L"ColorScheme_Black/Text"),
            RS_(L"ColorScheme_Red/Text"),
            RS_(L"ColorScheme_Green/Text"),
            RS_(L"ColorScheme_Yellow/Text"),
            RS_(L"ColorScheme_Blue/Text"),
            RS_(L"ColorScheme_Purple/Text"),
            RS_(L"ColorScheme_Cyan/Text"),
            RS_(L"ColorScheme_White/Text"),
            RS_(L"ColorScheme_BrightBlack/Text"),
            RS_(L"ColorScheme_BrightRed/Text"),
            RS_(L"ColorScheme_BrightGreen/Text"),
            RS_(L"ColorScheme_BrightYellow/Text"),
            RS_(L"ColorScheme_BrightBlue/Text"),
            RS_(L"ColorScheme_BrightPurple/Text"),
            RS_(L"ColorScheme_BrightCyan/Text"),
            RS_(L"ColorScheme_BrightWhite/Text")
        };

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
