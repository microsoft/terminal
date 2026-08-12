/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontConfig

Abstract:
- The implementation of the FontConfig winrt class. Provides settings related
  to the font settings of the terminal, for the terminal control.

Author(s):
- Pankaj Bhojwani - June 2021

--*/

#pragma once

#include "pch.h"
#include "FontConfig.g.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"
#include "MTSMSettings.h"
#include "IInheritable.h"
#include <DefaultSettings.h>

using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct FontConfig : FontConfigT<FontConfig>, IInheritable<FontConfig>
    {
    public:
        FontConfig(winrt::weak_ref<Profile> sourceProfile);
        static winrt::com_ptr<FontConfig> CopyFontInfo(const FontConfig* source, winrt::weak_ref<Profile> sourceProfile);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);
        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

        Model::Profile SourceProfile();

        // Generic setting access via SettingKey
        bool HasSetting(FontSettingKey key) const;
        void ClearSetting(FontSettingKey key);
        std::vector<FontSettingKey> CurrentSettings() const;

#define FONT_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::FontConfig, type, name, jsonKey, ##__VA_ARGS__)
        MTSM_FONT_SETTINGS_SCALARS(FONT_SETTINGS_INITIALIZE)
#undef FONT_SETTINGS_INITIALIZE

#define FONT_COLLECTION_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_JSON_BACKED_MAP_SETTING(Model::FontConfig, type, name, jsonKey, ##__VA_ARGS__)
        MTSM_FONT_SETTINGS_COLLECTIONS(FONT_COLLECTION_INITIALIZE)
#undef FONT_COLLECTION_INITIALIZE

    private:
        winrt::weak_ref<Profile> _sourceProfile;

        // Raw JSON for this layer (font sub-object shape).
        Json::Value _json{ Json::ValueType::objectValue };

        std::set<std::string> _changeLog;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, const bool isSet);

        void _ValidateThisLayer() const override;
    };
}
