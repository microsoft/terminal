/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once

#include "../inc/cppwinrt_utils.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

namespace ControlUnitTests
{
    class MockControlSettings : public winrt::implements<MockControlSettings, winrt::Microsoft::Terminal::Core::ICoreSettings, winrt::Microsoft::Terminal::Control::IControlSettings, winrt::Microsoft::Terminal::Core::ICoreAppearance, winrt::Microsoft::Terminal::Control::IControlAppearance>
    {
    public:
        MockControlSettings() = default;

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        WINRT_PROPERTY(til::color, DefaultForeground, DEFAULT_FOREGROUND);
        WINRT_PROPERTY(til::color, DefaultBackground, DEFAULT_BACKGROUND);
        WINRT_PROPERTY(til::color, SelectionBackground, DEFAULT_FOREGROUND);
        WINRT_PROPERTY(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        WINRT_PROPERTY(int32_t, InitialRows, 30);
        WINRT_PROPERTY(int32_t, InitialCols, 80);

        WINRT_PROPERTY(bool, SnapOnInput, true);
        WINRT_PROPERTY(bool, AltGrAliasing, true);
        WINRT_PROPERTY(til::color, CursorColor, DEFAULT_CURSOR_COLOR);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Core::CursorStyle, CursorShape, winrt::Microsoft::Terminal::Core::CursorStyle::Vintage);
        WINRT_PROPERTY(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        WINRT_PROPERTY(winrt::hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        WINRT_PROPERTY(bool, CopyOnSelect, false);
        WINRT_PROPERTY(bool, InputServiceWarning, true);
        WINRT_PROPERTY(bool, FocusFollowMouse, false);

        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, TabColor, nullptr);

        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);

        winrt::Microsoft::Terminal::Core::ICoreAppearance UnfocusedAppearance() { return {}; };

        WINRT_PROPERTY(bool, TrimBlockSelection, false);
        WINRT_PROPERTY(bool, DetectURLs, true);
        // ------------------------ End of Core Settings -----------------------

        WINRT_PROPERTY(winrt::hstring, ProfileName);
        WINRT_PROPERTY(bool, UseAcrylic, false);
        WINRT_PROPERTY(double, TintOpacity, 0.5);
        WINRT_PROPERTY(winrt::hstring, Padding, DEFAULT_PADDING);
        WINRT_PROPERTY(winrt::hstring, FontFace, L"Consolas");
        WINRT_PROPERTY(int32_t, FontSize, DEFAULT_FONT_SIZE);

        WINRT_PROPERTY(winrt::Windows::UI::Text::FontWeight, FontWeight);

        WINRT_PROPERTY(winrt::hstring, BackgroundImage);
        WINRT_PROPERTY(double, BackgroundImageOpacity, 1.0);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        WINRT_PROPERTY(winrt::Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr);

        WINRT_PROPERTY(winrt::hstring, Commandline);
        WINRT_PROPERTY(winrt::hstring, StartingDirectory);
        WINRT_PROPERTY(winrt::hstring, StartingTitle);
        WINRT_PROPERTY(bool, SuppressApplicationTitle);
        WINRT_PROPERTY(winrt::hstring, EnvironmentVariables);

        WINRT_PROPERTY(winrt::Microsoft::Terminal::Control::ScrollbarState, ScrollState, winrt::Microsoft::Terminal::Control::ScrollbarState::Visible);

        WINRT_PROPERTY(winrt::Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale);

        WINRT_PROPERTY(bool, RetroTerminalEffect, false);
        WINRT_PROPERTY(bool, ForceFullRepaintRendering, false);
        WINRT_PROPERTY(bool, SoftwareRendering, false);
        WINRT_PROPERTY(bool, ForceVTInput, false);

        WINRT_PROPERTY(winrt::hstring, PixelShaderPath);

    private:
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> _ColorTable;

    public:
        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept { return _ColorTable.at(index); }
        std::array<winrt::Microsoft::Terminal::Core::Color, 16> ColorTable() { return _ColorTable; }
        void ColorTable(std::array<winrt::Microsoft::Terminal::Core::Color, 16> /*colors*/) {}
    };
}
