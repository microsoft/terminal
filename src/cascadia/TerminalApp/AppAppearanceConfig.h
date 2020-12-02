// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppAppearanceConfig.g.h"
#include "..\inc\cppwinrt_utils.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

namespace winrt::TerminalApp::implementation
{
    struct AppAppearanceConfig : AppAppearanceConfigT<AppAppearanceConfig>
    {
        AppAppearanceConfig() = default;
        void ApplyColorScheme(const Microsoft::Terminal::Settings::Model::ColorScheme& scheme);
        uint32_t GetColorTableEntry(int32_t index) const noexcept;

        GETSET_PROPERTY(hstring, ColorSchemeName);
        GETSET_PROPERTY(uint32_t, DefaultForeground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, DefaultBackground, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_PROPERTY(uint32_t, SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_PROPERTY(uint32_t, CursorColor, DEFAULT_CURSOR_COLOR);
        GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Vintage);

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

    private:
        std::array<uint32_t, COLOR_TABLE_SIZE> _colorTable{};
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppAppearanceConfig);
}
