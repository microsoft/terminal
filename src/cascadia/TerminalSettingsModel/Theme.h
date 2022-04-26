/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Theme.hpp

Abstract:
- TODO!

Author(s):
- Mike Griese - March 2022

--*/
#pragma once

#include "../../inc/conattrs.hpp"
#include "DefaultSettings.h"
#include "IInheritable.h"
#include "MTSMSettings.h"

#include "ThemeColor.g.h"
#include "Theme.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ThemeColor : ThemeColorT<ThemeColor>
    {
    public:
        ThemeColor() noexcept;
        static winrt::Microsoft::Terminal::Settings::Model::ThemeColor FromColor(const winrt::Microsoft::Terminal::Core::Color& coreColor) noexcept;
        static winrt::Microsoft::Terminal::Settings::Model::ThemeColor FromAccent() noexcept;

        WINRT_PROPERTY(til::color, Color);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::ThemeColorType, ColorType);

    private:
    };

    struct Theme : ThemeT<Theme>
    {
    public:
        Theme() noexcept;
        Theme(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept;

        com_ptr<Theme> Copy() const;

        hstring ToString()
        {
            return Name();
        }

        static com_ptr<Theme> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        WINRT_PROPERTY(winrt::hstring, Name);

#define THEME_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    WINRT_PROPERTY(type, name, ##__VA_ARGS__)

        MTSM_THEME_SETTINGS(THEME_SETTINGS_INITIALIZE)
#undef THEME_SETTINGS_INITIALIZE

    private:
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ThemeColor);
    BASIC_FACTORY(Theme);
}
