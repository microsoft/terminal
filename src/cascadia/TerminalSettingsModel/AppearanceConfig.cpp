// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppearanceConfig.h"
#include "AppearanceConfig.g.cpp"
#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"
#include "Profile.h"
#include "MediaResourceSupport.h"

using namespace winrt::Microsoft::Terminal::Control;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view LegacyAcrylicTransparencyKey{ "acrylicOpacity" };
static constexpr std::string_view OpacityKey{ "opacity" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };

AppearanceConfig::AppearanceConfig(winrt::weak_ref<Model::Profile> sourceProfile) :
    _sourceProfile(std::move(sourceProfile))
{
}

winrt::com_ptr<AppearanceConfig> AppearanceConfig::CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Model::Profile> sourceProfile)
{
    auto appearance{ winrt::make_self<AppearanceConfig>(std::move(sourceProfile)) };
    appearance->_Foreground = source->_Foreground;
    appearance->_Background = source->_Background;
    appearance->_SelectionBackground = source->_SelectionBackground;
    appearance->_CursorColor = source->_CursorColor;
    appearance->_Opacity = source->_Opacity;

    appearance->_DarkColorSchemeName = source->_DarkColorSchemeName;
    appearance->_LightColorSchemeName = source->_LightColorSchemeName;

#define APPEARANCE_SETTINGS_COPY(type, name, jsonKey, ...) \
    appearance->_##name = source->_##name;
    MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_COPY)
#undef APPEARANCE_SETTINGS_COPY

    return appearance;
}

Json::Value AppearanceConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::SetValueForKey(json, OpacityKey, _Opacity, JsonUtils::OptionalConverter<float, IntAsFloatPercentConversionTrait>{});
    if (HasDarkColorSchemeName() || HasLightColorSchemeName())
    {
        // check if the setting is coming from the UI, if so grab the ColorSchemeName until the settings UI is fixed.
        if (_LightColorSchemeName != _DarkColorSchemeName)
        {
            JsonUtils::SetValueForKey(json["colorScheme"], "dark", _DarkColorSchemeName);
            JsonUtils::SetValueForKey(json["colorScheme"], "light", _LightColorSchemeName);
        }
        else
        {
            JsonUtils::SetValueForKey(json, "colorScheme", _DarkColorSchemeName);
        }
    }

#define APPEARANCE_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_TO_JSON)
#undef APPEARANCE_SETTINGS_TO_JSON

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
// - This is similar to Profile::LayerJson but for AppearanceConfig
// Arguments:
// - json: an object which should be a partial serialization of an AppearanceConfig object.
void AppearanceConfig::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    _logSettingIfSet(ForegroundKey, _Foreground.has_value());

    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    _logSettingIfSet(BackgroundKey, _Background.has_value());

    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    _logSettingIfSet(SelectionBackgroundKey, _SelectionBackground.has_value());

    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);
    _logSettingIfSet(CursorColorKey, _CursorColor.has_value());

    JsonUtils::GetValueForKey(json, LegacyAcrylicTransparencyKey, _Opacity);
    JsonUtils::GetValueForKey(json, OpacityKey, _Opacity, JsonUtils::OptionalConverter<float, IntAsFloatPercentConversionTrait>{});
    _logSettingIfSet(OpacityKey, _Opacity.has_value());

    if (json["colorScheme"].isString())
    {
        // to make the UI happy, set ColorSchemeName.
        JsonUtils::GetValueForKey(json, ColorSchemeKey, _DarkColorSchemeName);
        _LightColorSchemeName = _DarkColorSchemeName;
        _logSettingSet(ColorSchemeKey);
    }
    else if (json["colorScheme"].isObject())
    {
        // to make the UI happy, set ColorSchemeName to whatever the dark value is.
        JsonUtils::GetValueForKey(json["colorScheme"], "dark", _DarkColorSchemeName);
        JsonUtils::GetValueForKey(json["colorScheme"], "light", _LightColorSchemeName);

        _logSettingSet("colorScheme.dark");
        _logSettingSet("colorScheme.light");
    }

#define APPEARANCE_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);           \
    _logSettingIfSet(jsonKey, _##name.has_value());

    MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_LAYER_JSON)
#undef APPEARANCE_SETTINGS_LAYER_JSON
}

winrt::Microsoft::Terminal::Settings::Model::Profile AppearanceConfig::SourceProfile()
{
    return _sourceProfile.get();
}

std::tuple<winrt::hstring, Model::OriginTag> AppearanceConfig::_getSourceProfileBasePathAndOrigin() const
{
    winrt::hstring sourceBasePath{};
    OriginTag origin{ OriginTag::None };
    if (const auto profile{ _sourceProfile.get() })
    {
        const auto profileImpl{ winrt::get_self<implementation::Profile>(profile) };
        sourceBasePath = profileImpl->SourceBasePath;
        origin = profileImpl->Origin();
    }
    return { sourceBasePath, origin };
}

void AppearanceConfig::ResolveMediaResources(const Model::MediaResourceResolver& resolver)
{
    if (const auto [source, resource] = _getBackgroundImagePathOverrideSourceAndValueImpl(); source && resource && *resource)
    {
        const auto [sourceBasePath, sourceOrigin]{ source->_getSourceProfileBasePathAndOrigin() };
        ResolveMediaResource(sourceOrigin, sourceBasePath, *resource, resolver);
    }
    if (const auto [source, resource]{ _getPixelShaderPathOverrideSourceAndValueImpl() }; source && resource && *resource)
    {
        const auto [sourceBasePath, sourceOrigin]{ source->_getSourceProfileBasePathAndOrigin() };
        ResolveMediaResource(sourceOrigin, sourceBasePath, *resource, resolver);
    }
    if (const auto [source, resource]{ _getPixelShaderImagePathOverrideSourceAndValueImpl() }; source && resource && *resource)
    {
        const auto [sourceBasePath, sourceOrigin]{ source->_getSourceProfileBasePathAndOrigin() };
        ResolveMediaResource(sourceOrigin, sourceBasePath, *resource, resolver);
    }
}

void AppearanceConfig::_logSettingSet(const std::string_view& setting)
{
    _changeLog.emplace(setting);
}

void AppearanceConfig::_logSettingIfSet(const std::string_view& setting, const bool isSet)
{
    if (isSet)
    {
        _logSettingSet(setting);
    }
}

void AppearanceConfig::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
{
    for (const auto& setting : _changeLog)
    {
        changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
    }
}
