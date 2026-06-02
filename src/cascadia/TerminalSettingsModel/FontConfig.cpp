// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FontConfig.h"
#include "FontConfig.g.cpp"

#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view FontInfoKey{ "font" };
static constexpr std::string_view FontAxesKey{ "axes" };
static constexpr std::string_view FontFeaturesKey{ "features" };
static constexpr std::string_view LegacyFontFaceKey{ "fontFace" };
static constexpr std::string_view LegacyFontSizeKey{ "fontSize" };
static constexpr std::string_view LegacyFontWeightKey{ "fontWeight" };

winrt::Microsoft::Terminal::Settings::Model::implementation::FontConfig::FontConfig(winrt::weak_ref<Profile> sourceProfile) :
    _sourceProfile(std::move(sourceProfile))
{
}

winrt::com_ptr<FontConfig> FontConfig::CopyFontInfo(const FontConfig* source, winrt::weak_ref<Profile> sourceProfile)
{
    auto fontInfo{ winrt::make_self<FontConfig>(std::move(sourceProfile)) };

    fontInfo->_json = source->_json;

    // FontAxes and FontFeatures are now JSON-backed — handled by _json copy above.
    // No manual deep-copy needed; the JSON-backed getter returns a fresh deserialized
    // collection each time.

    return fontInfo;
}

Json::Value FontConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    // MTSM font settings: copy from _json (the source of truth)
#define FONT_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::CopyKeyIfPresent(_json, json, jsonKey);
    MTSM_FONT_SETTINGS(FONT_SETTINGS_TO_JSON)
#undef FONT_SETTINGS_TO_JSON

    return json;
}

bool FontConfig::HasSetting(FontSettingKey key) const
{
    switch (key)
    {
#define _FONT_HAS_SETTING(type, name, jsonKey, ...) \
    case FontSettingKey::name:                       \
        return Has##name();
        MTSM_FONT_SETTINGS(_FONT_HAS_SETTING)
#undef _FONT_HAS_SETTING
    default:
        return false;
    }
}

void FontConfig::ClearSetting(FontSettingKey key)
{
    switch (key)
    {
#define _FONT_CLEAR_SETTING(type, name, jsonKey, ...) \
    case FontSettingKey::name:                         \
        Clear##name();                                \
        break;
        MTSM_FONT_SETTINGS(_FONT_CLEAR_SETTING)
#undef _FONT_CLEAR_SETTING
    default:
        break;
    }
}

std::vector<FontSettingKey> FontConfig::CurrentSettings() const
{
    std::vector<FontSettingKey> result;
    for (auto i = 0; i < static_cast<int>(FontSettingKey::SETTINGS_SIZE); i++)
    {
        const auto key = static_cast<FontSettingKey>(i);
        if (HasSetting(key))
        {
            result.push_back(key);
        }
    }
    return result;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// - Optional values that are set to `null` in the json object
//   will be set to nullopt.
// - This is similar to Profile::LayerJson but for FontConfig
// Arguments:
// - json: an object which should be a partial serialization of a FontConfig object.
void FontConfig::LayerJson(const Json::Value& json)
{
    // Legacy users may not have a font object defined in their profile,
    // so check for that before we decide how to parse this
    if (json.isMember(JsonKey(FontInfoKey)))
    {
        // A font object is defined, use that
        const auto fontInfoJson = json[JsonKey(FontInfoKey)];

        // Merge the font sub-object into stored _json (font-object shape).
        JsonUtils::MergeJsonKeys(fontInfoJson, _json);

        // MTSM font settings are now JSON-backed (including FontAxes/FontFeatures).
        // Values are already in _json. We only need to log which settings were set.
#define FONT_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    _logSettingIfSet(jsonKey, fontInfoJson.isMember(jsonKey) && !fontInfoJson[jsonKey].isNull());

        MTSM_FONT_SETTINGS(FONT_SETTINGS_LAYER_JSON)
#undef FONT_SETTINGS_LAYER_JSON
    }
    else
    {
        // No font object is defined — normalize legacy flat keys into font-object shape.
        if (json.isMember(JsonKey(LegacyFontFaceKey)))
        {
            _json["face"] = json[JsonKey(LegacyFontFaceKey)];
        }
        if (json.isMember(JsonKey(LegacyFontSizeKey)))
        {
            _json["size"] = json[JsonKey(LegacyFontSizeKey)];
        }
        if (json.isMember(JsonKey(LegacyFontWeightKey)))
        {
            _json["weight"] = json[JsonKey(LegacyFontWeightKey)];
        }

        // Log settings as if they were a part of the font object
        _logSettingIfSet("face", json.isMember(JsonKey(LegacyFontFaceKey)));
        _logSettingIfSet("size", json.isMember(JsonKey(LegacyFontSizeKey)));
        _logSettingIfSet("weight", json.isMember(JsonKey(LegacyFontWeightKey)));
    }

    _ValidateThisLayer();
}

void FontConfig::_ValidateThisLayer() const
{
    MTSM_FONT_SETTINGS(MTSM_VALIDATE_SETTING)
}

winrt::Microsoft::Terminal::Settings::Model::Profile FontConfig::SourceProfile()
{
    return _sourceProfile.get();
}

void FontConfig::_logSettingSet(const std::string_view& setting)
{
    if (setting == FontAxesKey && HasFontAxes())
    {
        for (const auto& [mapKey, _] : FontAxes())
        {
            _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, til::u16u8(mapKey)));
        }
    }
    else if (setting == FontFeaturesKey && HasFontFeatures())
    {
        for (const auto& [mapKey, _] : FontFeatures())
        {
            _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, til::u16u8(mapKey)));
        }
    }
    else
    {
        _changeLog.emplace(setting);
    }
}

void FontConfig::_logSettingIfSet(const std::string_view& setting, const bool isSet)
{
    if (isSet)
    {
        _logSettingSet(setting);
    }
}

void FontConfig::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
{
    for (const auto& setting : _changeLog)
    {
        changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
    }
}
