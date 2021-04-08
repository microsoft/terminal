/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AppearanceConfig

Abstract:
- The implementation of the AppearanceConfig winrt class. Provides settings related
  to the appearance of the terminal, in both terminal control and terminal core.

Author(s):
- Pankaj Bhojwani - Nov 2020

--*/

#pragma once

#include "pch.h"
#include "AppearanceConfig.g.h"
#include "JsonUtils.h"
#include "../inc/cppwinrt_utils.h"
#include "IInheritable.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig>, IInheritable<AppearanceConfig>
    {
    public:
        AppearanceConfig(const winrt::weak_ref<Profile> sourceProfile);
        static winrt::com_ptr<AppearanceConfig> CopyAppearance(const winrt::com_ptr<AppearanceConfig> source, const winrt::weak_ref<Profile> sourceProfile);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

        Model::Profile SourceProfile();

        winrt::hstring ExpandedBackgroundImagePath();

        INHERITABLE_SETTING(Model::IAppearanceConfig, ConvergedAlignment, BackgroundImageAlignment, ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center);

        INHERITABLE_SETTING(Model::IAppearanceConfig, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, ColorSchemeName, L"Campbell");
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Foreground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Background, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, SelectionBackground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, CursorColor, nullptr);
        INHERITABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::CursorStyle, CursorShape, Microsoft::Terminal::Core::CursorStyle::Bar);
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, BackgroundImagePath);

        INHERITABLE_SETTING(Model::IAppearanceConfig, double, BackgroundImageOpacity, 1.0);
        INHERITABLE_SETTING(Model::IAppearanceConfig, Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::UniformToFill);

        INHERITABLE_SETTING(Model::IAppearanceConfig, bool, RetroTerminalEffect, false);
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, PixelShaderPath, L"");

    private:
        winrt::weak_ref<Profile> _sourceProfile;
    };
}
