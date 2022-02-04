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

    ColorSchemeViewModel::ColorSchemeViewModel(Model::ColorScheme scheme) :
        _CurrentNonBrightColorTable{ single_threaded_observable_vector<Editor::ColorTableEntry>() },
        _CurrentBrightColorTable { single_threaded_observable_vector<Editor::ColorTableEntry>() }
    {
        for (uint8_t i = 0; i < ColorTableSize; ++i)
        {
            til::color currentColor{ scheme.Table()[i] };
            const auto& entry{ winrt::make<ColorTableEntry>(i, currentColor) };
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
    }
}
