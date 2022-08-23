/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Theme.hpp

Abstract:
- A Theme represents a collection of settings which control the appearance of
  the Terminal window itself. Things like the color of the titlebar, the style
  of the tabs.

Author(s):
- Mike Griese - March 2022

--*/
#pragma once

#include "MTSMSettings.h"

#include "ThemeColor.g.h"
#include "WindowTheme.g.h"
#include "TabRowTheme.g.h"
#include "TabTheme.g.h"
#include "Theme.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ThemeColor : ThemeColorT<ThemeColor>
    {
    public:
        ThemeColor() noexcept = default;
        static winrt::Microsoft::Terminal::Settings::Model::ThemeColor FromColor(const winrt::Microsoft::Terminal::Core::Color& coreColor) noexcept;
        static winrt::Microsoft::Terminal::Settings::Model::ThemeColor FromAccent() noexcept;
        static winrt::Microsoft::Terminal::Settings::Model::ThemeColor FromTerminalBackground() noexcept;

        static til::color ColorFromBrush(const winrt::Windows::UI::Xaml::Media::Brush& brush);

        winrt::Windows::UI::Xaml::Media::Brush Evaluate(const winrt::Windows::UI::Xaml::ResourceDictionary& res,
                                                        const winrt::Windows::UI::Xaml::Media::Brush& terminalBackground,
                                                        const bool forTitlebar);

        WINRT_PROPERTY(til::color, Color);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::ThemeColorType, ColorType);
    };

#define THEME_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    WINRT_PROPERTY(type, name, ##__VA_ARGS__)

#define THEME_OBJECT(className, macro)         \
    struct className : className##T<className> \
    {                                          \
        winrt::com_ptr<className> Copy();      \
        Json::Value ToJson();                  \
                                               \
        macro(THEME_SETTINGS_INITIALIZE);      \
    };

    THEME_OBJECT(WindowTheme, MTSM_THEME_WINDOW_SETTINGS);
    THEME_OBJECT(TabRowTheme, MTSM_THEME_TABROW_SETTINGS);
    THEME_OBJECT(TabTheme, MTSM_THEME_TAB_SETTINGS);

    struct Theme : ThemeT<Theme>
    {
    public:
        Theme() = default;
        Theme(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept;

        com_ptr<Theme> Copy() const;

        hstring ToString();

        static com_ptr<Theme> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        winrt::Windows::UI::Xaml::ElementTheme RequestedTheme() const noexcept;

        WINRT_PROPERTY(winrt::hstring, Name);

        MTSM_THEME_SETTINGS(THEME_SETTINGS_INITIALIZE)

    private:
    };

#undef THEME_SETTINGS_INITIALIZE
#undef THEME_OBJECT
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ThemeColor);
    BASIC_FACTORY(Theme);
}
