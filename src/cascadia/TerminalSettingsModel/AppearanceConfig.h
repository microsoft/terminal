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
#include "TerminalSettingsSerializationHelpers.h"
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

        // Generic setting access via SettingKey
        bool HasSetting(AppearanceSettingKey key) const;
        void ClearSetting(AppearanceSettingKey key);
        std::vector<AppearanceSettingKey> CurrentSettings() const;

        // Nullable color settings (JSON-backed)
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Foreground, "foreground", nullptr)
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, Background, "background", nullptr)
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, SelectionBackground, "selectionBackground", nullptr)
        INHERITABLE_NULLABLE_SETTING(Model::IAppearanceConfig, Microsoft::Terminal::Core::Color, CursorColor, "cursorColor", nullptr)

        // Opacity: JSON-backed with normalization (int percent → float in LayerJson)
        INHERITABLE_SETTING(Model::IAppearanceConfig, float, Opacity, "opacity", 1.0f)

        // ColorScheme: dark/light are two independent JSON-backed settings
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, DarkColorSchemeName, "colorSchemeDark", L"Campbell")
        INHERITABLE_SETTING(Model::IAppearanceConfig, hstring, LightColorSchemeName, "colorSchemeLight", L"Campbell")

#define APPEARANCE_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::IAppearanceConfig, type, name, jsonKey, ##__VA_ARGS__)
        MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_INITIALIZE)
#undef APPEARANCE_SETTINGS_INITIALIZE

        // IMediaResource settings with backing fields for resolution lifecycle.
        // See IInheritable.h for the INHERITABLE_MEDIA_RESOURCE_SETTING macro definition.
        INHERITABLE_MEDIA_RESOURCE_SETTING(Model::IAppearanceConfig, PixelShaderPath, "experimental.pixelShaderPath", implementation::MediaResource::Empty())
        INHERITABLE_MEDIA_RESOURCE_SETTING(Model::IAppearanceConfig, PixelShaderImagePath, "experimental.pixelShaderImagePath", implementation::MediaResource::Empty())
        INHERITABLE_MEDIA_RESOURCE_SETTING(Model::IAppearanceConfig, BackgroundImagePath, "backgroundImage", implementation::MediaResource::Empty())

    private:
        winrt::weak_ref<Profile> _sourceProfile;

        // Raw JSON for this layer (appearance-relevant keys only).
        Json::Value _json{ Json::ValueType::objectValue };

        std::set<std::string> _changeLog;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, const bool isSet);

        void _ValidateThisLayer() const override;

        std::tuple<winrt::hstring, Model::OriginTag> _getSourceProfileBasePathAndOrigin() const;
    };
}
