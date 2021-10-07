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

#include "AppearanceConfig.g.h"
#include "JsonUtils.h"
#include "IInheritable.h"
#include "SettingsUtils.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig>, IInheritable<AppearanceConfig>
    {
    public:
        AppearanceConfig(winrt::weak_ref<Profile> sourceProfile);
        static winrt::com_ptr<AppearanceConfig> CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Profile> sourceProfile);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

        Model::Profile SourceProfile();

        winrt::hstring ExpandedBackgroundImagePath();

        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Foreground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Background, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, SelectionBackground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, CursorColor, nullptr);

#define APPEARANCE_CONTROL_SETTINGS_2_INITIALIZE(type, name, ...) \
    INHERITABLE_SETTING(Model::IAppearanceConfig, type, name, ##__VA_ARGS__)
        APPEARANCE_CONTROL_SETTINGS_2(APPEARANCE_CONTROL_SETTINGS_2_INITIALIZE)
#undef APPEARANCE_CONTROL_SETTINGS_2_INITIALIZE

#define APPEARANCE_CONTROL_SETTINGS_INITIALIZE(type, name, ...) \
    INHERITABLE_SETTING(Model::IAppearanceConfig, type, name, ##__VA_ARGS__)
        APPEARANCE_CONTROL_SETTINGS(APPEARANCE_CONTROL_SETTINGS_INITIALIZE)
#undef APPEARANCE_CONTROL_SETTINGS_INITIALIZE

    private:
        winrt::weak_ref<Profile> _sourceProfile;
    };
}
