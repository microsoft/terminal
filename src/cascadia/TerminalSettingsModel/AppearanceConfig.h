// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AppearanceConfig.g.h"
#include "JsonUtils.h"
#include "../inc/cppwinrt_utils.h"
#include "IInheritable.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig>, IInheritable<Profile>
    {
    public:
        AppearanceConfig();

        static winrt::com_ptr<AppearanceConfig> CopyAppearance(winrt::com_ptr<IAppearanceConfig> source);
        Json::Value ToJson() const;

        void LayerJson(const Json::Value& json);
        winrt::hstring ExpandedBackgroundImagePath();

        GETSET_SETTING(ConvergedAlignment, BackgroundImageAlignment, ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center);

        GETSET_SETTING(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        GETSET_SETTING(hstring, ColorSchemeName, L"Campbell");
        GETSET_NULLABLE_SETTING(Windows::UI::Color, Foreground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, Background, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, SelectionBackground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, CursorColor, nullptr);
        GETSET_SETTING(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        GETSET_SETTING(hstring, BackgroundImagePath);

        GETSET_SETTING(double, BackgroundImageOpacity, 1.0);
        GETSET_SETTING(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::UniformToFill);

    private:
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(AppearanceConfig);
}
