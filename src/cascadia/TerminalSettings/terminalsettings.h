/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TerminalSettings.h

Abstract:
- The implementation of the TerminalSettings winrt class. Provides both
        terminal control settings and terminal core settings.
Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "TerminalSettings.g.h"
#include "../inc/cppwinrt_utils.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    struct TerminalSettings : TerminalSettingsT<TerminalSettings>
    {
        TerminalSettings() = default;

// TECHNICALLY, the hstring copy assignment can throw, but the GETSET_PROPERTY
// macro defines the operator as `noexcept`. We're not really worried about it,
// because the only time it will throw is when we're out of memory, and then
// we've got much worse problems. So just suppress that warning for now.
#pragma warning(push)
#pragma warning(disable : 26447)
        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        // Get/Set ColorTableEntry needs to be implemented manually, to get a
        // particular value from the array.
        uint32_t GetColorTableEntry(int32_t index) const noexcept;
        void SetColorTableEntry(int32_t index, uint32_t value);

        // the RowsToScroll getter needs to be implemented manually, so it can
        // default to SPI_GETWHEELSCROLLLINES
        int32_t RowsToScroll() noexcept;
        void RowsToScroll(int32_t value) noexcept;

        GETSET_PROPERTY(uint32_t, DefaultForeground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, DefaultBackground, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_PROPERTY(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        GETSET_PROPERTY(int32_t, InitialRows, 30);
        GETSET_PROPERTY(int32_t, InitialCols, 80);

        GETSET_PROPERTY(bool, SnapOnInput, true);
        GETSET_PROPERTY(bool, AltGrAliasing, true);
        GETSET_PROPERTY(uint32_t, CursorColor, DEFAULT_CURSOR_COLOR);
        GETSET_PROPERTY(CursorStyle, CursorShape, CursorStyle::Vintage);
        GETSET_PROPERTY(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        GETSET_PROPERTY(hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        GETSET_PROPERTY(bool, CopyOnSelect, false);

        // ------------------------ End of Core Settings -----------------------

        GETSET_PROPERTY(hstring, ProfileName);
        GETSET_PROPERTY(bool, UseAcrylic, false);
        GETSET_PROPERTY(double, TintOpacity, 0.5);
        GETSET_PROPERTY(hstring, Padding, DEFAULT_PADDING);
        GETSET_PROPERTY(hstring, FontFace, DEFAULT_FONT_FACE);
        GETSET_PROPERTY(int32_t, FontSize, DEFAULT_FONT_SIZE);

        GETSET_PROPERTY(winrt::Windows::UI::Text::FontWeight, FontWeight);

        GETSET_PROPERTY(hstring, BackgroundImage);
        GETSET_PROPERTY(double, BackgroundImageOpacity, 1.0);

        GETSET_PROPERTY(winrt::Windows::UI::Xaml::Media::Stretch,
                        BackgroundImageStretchMode,
                        winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::HorizontalAlignment,
                        BackgroundImageHorizontalAlignment,
                        winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::VerticalAlignment,
                        BackgroundImageVerticalAlignment,
                        winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        GETSET_PROPERTY(IKeyBindings, KeyBindings, nullptr);

        GETSET_PROPERTY(hstring, Commandline);
        GETSET_PROPERTY(hstring, StartingDirectory);
        GETSET_PROPERTY(hstring, StartingTitle);
        GETSET_PROPERTY(bool, SuppressApplicationTitle);
        GETSET_PROPERTY(hstring, EnvironmentVariables);

        GETSET_PROPERTY(ScrollbarState, ScrollState, ScrollbarState::Visible);

        GETSET_PROPERTY(TextAntialiasingMode, AntialiasingMode, TextAntialiasingMode::Grayscale);

        GETSET_PROPERTY(bool, RetroTerminalEffect, false);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
        GETSET_PROPERTY(bool, SoftwareRendering, false);
        GETSET_PROPERTY(bool, ForceVTInput, false);

#pragma warning(pop)

    private:
        std::array<uint32_t, COLOR_TABLE_SIZE> _colorTable{};
        int32_t _rowsToScroll{ 0 };
    };
}

namespace winrt::Microsoft::Terminal::Settings::factory_implementation
{
    BASIC_FACTORY(TerminalSettings);
}
