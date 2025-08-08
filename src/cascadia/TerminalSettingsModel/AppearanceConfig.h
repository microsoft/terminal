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
#include "MTSMSettings.h"
#include "MediaResourceSupport.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig, IMediaResourceContainer>, IInheritable<AppearanceConfig>
    {
    public:
        AppearanceConfig(winrt::weak_ref<Profile> sourceProfile);
        static winrt::com_ptr<AppearanceConfig> CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Profile> sourceProfile);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);
        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

        Model::Profile SourceProfile();

        void ResolveMediaResources(const Model::MediaResourceResolver& resolver);

        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Foreground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Background, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, SelectionBackground, nullptr);
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, CursorColor, nullptr);
        INHERITABLE_SETTING(Model::IAppearanceConfig, float, Opacity, 1.0f);

        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, DarkColorSchemeName, L"Campbell");
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, LightColorSchemeName, L"Campbell");

#define APPEARANCE_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::IAppearanceConfig, type, name, ##__VA_ARGS__)
        MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_INITIALIZE)
#undef APPEARANCE_SETTINGS_INITIALIZE

    private:
        winrt::weak_ref<Profile> _sourceProfile;
        std::set<std::string> _changeLog;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, const bool isSet);

        std::tuple<winrt::hstring, Model::OriginTag> _getSourceProfileBasePathAndOrigin() const;
    };
}
