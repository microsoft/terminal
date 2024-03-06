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

#define FONT_SETTINGS_COPY(type, name, jsonKey, ...) \
    fontInfo->_##name = source->_##name;
    MTSM_FONT_SETTINGS(FONT_SETTINGS_COPY)
#undef FONT_SETTINGS_COPY

    // We cannot simply copy the font axes and features with `fontInfo->_FontAxes = source->_FontAxes;`
    // since that'll just create a reference; we have to manually copy the values.
    if (source->_FontAxes)
    {
        std::map<winrt::hstring, float> fontAxes;
        for (const auto keyValuePair : source->_FontAxes.value())
        {
            fontAxes.insert(std::pair<winrt::hstring, float>(keyValuePair.Key(), keyValuePair.Value()));
        }
        fontInfo->_FontAxes = winrt::single_threaded_map(std::move(fontAxes));
    }

    if (source->_FontFeatures)
    {
        std::map<winrt::hstring, uint32_t> fontFeatures;
        for (const auto keyValuePair : source->_FontFeatures.value())
        {
            fontFeatures.insert(std::pair<winrt::hstring, uint32_t>(keyValuePair.Key(), keyValuePair.Value()));
        }
        fontInfo->_FontFeatures = winrt::single_threaded_map(std::move(fontFeatures));
    }

    return fontInfo;
}

Json::Value FontConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

#define FONT_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_FONT_SETTINGS(FONT_SETTINGS_TO_JSON)
#undef FONT_SETTINGS_TO_JSON

    return json;
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
#define FONT_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(fontInfoJson, jsonKey, _##name);
        MTSM_FONT_SETTINGS(FONT_SETTINGS_LAYER_JSON)
#undef FONT_SETTINGS_LAYER_JSON
    }
    else
    {
        // No font object is defined
        JsonUtils::GetValueForKey(json, LegacyFontFaceKey, _FontFace);
        JsonUtils::GetValueForKey(json, LegacyFontSizeKey, _FontSize);
        JsonUtils::GetValueForKey(json, LegacyFontWeightKey, _FontWeight);
    }
}

winrt::Microsoft::Terminal::Settings::Model::Profile FontConfig::SourceProfile()
{
    return _sourceProfile.get();
}
