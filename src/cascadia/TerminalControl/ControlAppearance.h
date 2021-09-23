/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include "../../inc/cppwinrt_utils.h"

#include <DefaultSettings.h>
#include <conattrs.hpp>

// using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;
// using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // --------------------------- Core Appearance ---------------------------
    //  All of these settings are defined in ICoreSettings.
#define CORE_APPEARANCE_SETTINGS(X)                                                                                       \
    X(til::color, DefaultForeground, DEFAULT_FOREGROUND)                                                                  \
    X(til::color, DefaultBackground, DEFAULT_BACKGROUND)                                                                  \
    X(til::color, CursorColor, DEFAULT_CURSOR_COLOR)                                                                      \
    X(winrt::Microsoft::Terminal::Core::CursorStyle, CursorShape, winrt::Microsoft::Terminal::Core::CursorStyle::Vintage) \
    X(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT)                                                                      \
    X(bool, IntenseIsBright, true)

    // --------------------------- Control Appearance ---------------------------
    //  All of these settings are defined in IControlSettings.
#define CONTROL_APPEARANCE_SETTINGS(X)                                                                                                          \
    X(til::color, SelectionBackground, DEFAULT_FOREGROUND)                                                                                      \
    X(double, Opacity, 1.0)                                                                                                                     \
    X(winrt::hstring, BackgroundImage)                                                                                                          \
    X(double, BackgroundImageOpacity, 1.0)                                                                                                      \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill)            \
    X(winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center) \
    X(winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center)       \
    X(bool, IntenseIsBold)                                                                                                                      \
    X(bool, RetroTerminalEffect, false)                                                                                                         \
    X(winrt::hstring, PixelShaderPath)

    struct ControlAppearance : public winrt::implements<ControlAppearance, Microsoft::Terminal::Core::ICoreAppearance, Microsoft::Terminal::Control::IControlAppearance>
    {
#define CORE_SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_APPEARANCE_SETTINGS(CORE_SETTINGS_GEN)
#undef CORE_SETTINGS_GEN

#define CONTROL_SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CONTROL_APPEARANCE_SETTINGS(CONTROL_SETTINGS_GEN)
#undef CONTROL_SETTINGS_GEN

    private:
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> _ColorTable;

    public:
        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept { return _ColorTable.at(index); }
        std::array<winrt::Microsoft::Terminal::Core::Color, 16> ColorTable() { return _ColorTable; }
        void ColorTable(std::array<winrt::Microsoft::Terminal::Core::Color, 16> /*colors*/) {}

        ControlAppearance(Control::IControlAppearance appearance)
        {
#define COPY_SETTING(type, name, ...) _##name = appearance.name();
            CORE_APPEARANCE_SETTINGS(COPY_SETTING)
            CONTROL_APPEARANCE_SETTINGS(COPY_SETTING)
#undef COPY_SETTING
        }
    };
}

// namespace winrt::Microsoft::Terminal::Control::factory_implementation
// {
//     BASIC_FACTORY(ControlSettings);
// }
