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
static constexpr std::string_view FontFaceKey{ "face" };
static constexpr std::string_view FontSizeKey{ "size" };
static constexpr std::string_view FontWeightKey{ "weight" };
static constexpr std::string_view FontFeaturesKey{ "features" };
static constexpr std::string_view FontAxesKey{ "axes" };
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
    fontInfo->_FontFace = source->_FontFace;
    fontInfo->_FontSize = source->_FontSize;
    fontInfo->_FontWeight = source->_FontWeight;
    fontInfo->_FontAxes = source->_FontAxes;
    fontInfo->_FontFeatures = source->_FontFeatures;
    return fontInfo;
}

Json::Value FontConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, FontFaceKey, _FontFace);
    JsonUtils::SetValueForKey(json, FontSizeKey, _FontSize);
    JsonUtils::SetValueForKey(json, FontWeightKey, _FontWeight);
    JsonUtils::SetValueForKey(json, FontAxesKey, _FontAxes);
    JsonUtils::SetValueForKey(json, FontFeaturesKey, _FontFeatures);

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
        JsonUtils::GetValueForKey(fontInfoJson, FontFaceKey, _FontFace);
        JsonUtils::GetValueForKey(fontInfoJson, FontSizeKey, _FontSize);
        JsonUtils::GetValueForKey(fontInfoJson, FontWeightKey, _FontWeight);
        JsonUtils::GetValueForKey(fontInfoJson, FontFeaturesKey, _FontFeatures);
        JsonUtils::GetValueForKey(fontInfoJson, FontAxesKey, _FontAxes);
    }
    else
    {
        // No font object is defined
        JsonUtils::GetValueForKey(json, LegacyFontFaceKey, _FontFace);
        JsonUtils::GetValueForKey(json, LegacyFontSizeKey, _FontSize);
        JsonUtils::GetValueForKey(json, LegacyFontWeightKey, _FontWeight);
    }
}

bool FontConfig::HasAnyOptionSet() const
{
    return HasFontFace() || HasFontSize() || HasFontWeight();
}

winrt::Microsoft::Terminal::Settings::Model::Profile FontConfig::SourceProfile()
{
    return _sourceProfile.get();
}
