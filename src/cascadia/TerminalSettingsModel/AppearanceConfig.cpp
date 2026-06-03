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
// Internal _json keys that back the two independent dark/light settings. The polymorphic
// on-disk "colorScheme" key is translated to/from these only in LayerJson/ToJson; these
// keys themselves are never written to disk.
static constexpr std::string_view DarkColorSchemeNameKey{ "colorSchemeDark" };
static constexpr std::string_view LightColorSchemeNameKey{ "colorSchemeLight" };

AppearanceConfig::AppearanceConfig(winrt::weak_ref<Model::Profile> sourceProfile) :
    _sourceProfile(std::move(sourceProfile))
{
}

winrt::com_ptr<AppearanceConfig> AppearanceConfig::CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Model::Profile> sourceProfile)
{
    auto appearance{ winrt::make_self<AppearanceConfig>(std::move(sourceProfile)) };

    appearance->_json = source->_json;

    // JSON-backed settings (Foreground, Background, SelectionBackground, CursorColor,
    // Opacity, DarkColorSchemeName, LightColorSchemeName, MTSM settings) all live in
    // _json, which is already deep-copied above.

    // Complex/mutable settings — backing fields for resolution lifecycle.
    // _json (copied above) is the source of truth; backing fields hold resolved runtime state.
    appearance->_PixelShaderPath = source->_PixelShaderPath;
    appearance->_PixelShaderImagePath = source->_PixelShaderImagePath;
    appearance->_BackgroundImagePath = source->_BackgroundImagePath;

    return appearance;
}

Json::Value AppearanceConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    // Nullable color settings: key presence matters (explicit null is valid)
    for (const auto& key : { ForegroundKey, BackgroundKey, SelectionBackgroundKey, CursorColorKey })
    {
        JsonUtils::CopyKeyIfPresent(_json, json, key);
    }

    // Opacity: _json stores it as a float (0.0–1.0). Serialize it to
    // integer-percent form (i.e. 0.5 -> 50) via IntAsFloatPercentConversionTrait.
    if (_json.isMember(JsonKey(OpacityKey)) && !_json[JsonKey(OpacityKey)].isNull())
    {
        const auto opacityValue{ JsonUtils::GetValue<float>(_json[JsonKey(OpacityKey)]) };
        JsonUtils::SetValueForKey(json, OpacityKey, opacityValue, IntAsFloatPercentConversionTrait{});
    }

    // ColorScheme: collapse the two independent dark/light settings back to the polymorphic
    // on-disk "colorScheme" key. Emit a string when both this-layer sides are set and equal,
    // otherwise an object containing only the side(s) this layer sets. (Mirrors main.)
    {
        const auto hasDark{ _json.isMember(JsonKey(DarkColorSchemeNameKey)) && !_json[JsonKey(DarkColorSchemeNameKey)].isNull() };
        const auto hasLight{ _json.isMember(JsonKey(LightColorSchemeNameKey)) && !_json[JsonKey(LightColorSchemeNameKey)].isNull() };
        if (hasDark || hasLight)
        {
            const auto& dark{ _json[JsonKey(DarkColorSchemeNameKey)] };
            const auto& light{ _json[JsonKey(LightColorSchemeNameKey)] };
            if (hasDark && hasLight && dark == light)
            {
                json[JsonKey(ColorSchemeKey)] = dark;
            }
            else
            {
                if (hasDark)
                {
                    json[JsonKey(ColorSchemeKey)]["dark"] = dark;
                }
                if (hasLight)
                {
                    json[JsonKey(ColorSchemeKey)]["light"] = light;
                }
            }
        }
    }

    // MTSM appearance settings: copy from _json (the source of truth)
#define APPEARANCE_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::CopyKeyIfPresent(_json, json, jsonKey);
    MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_TO_JSON)
#undef APPEARANCE_SETTINGS_TO_JSON

    // Complex/mutable settings — read from _json (source of truth), not backing fields
    JsonUtils::CopyKeyIfPresent(_json, json, "experimental.pixelShaderPath");
    JsonUtils::CopyKeyIfPresent(_json, json, "experimental.pixelShaderImagePath");
    JsonUtils::CopyKeyIfPresent(_json, json, "backgroundImage");

    return json;
}

bool AppearanceConfig::HasSetting(AppearanceSettingKey key) const
{
    switch (key)
    {
#define _APPEARANCE_HAS_SETTING(type, name, jsonKey, ...) \
    case AppearanceSettingKey::name:                      \
        return Has##name();
        MTSM_APPEARANCE_SETTINGS(_APPEARANCE_HAS_SETTING)
#undef _APPEARANCE_HAS_SETTING
    case AppearanceSettingKey::_Foreground:
        return HasForeground();
    case AppearanceSettingKey::_Background:
        return HasBackground();
    case AppearanceSettingKey::_SelectionBackground:
        return HasSelectionBackground();
    case AppearanceSettingKey::_CursorColor:
        return HasCursorColor();
    case AppearanceSettingKey::_Opacity:
        return HasOpacity();
    case AppearanceSettingKey::_DarkColorSchemeName:
        return HasDarkColorSchemeName();
    case AppearanceSettingKey::_LightColorSchemeName:
        return HasLightColorSchemeName();
    case AppearanceSettingKey::_PixelShaderPath:
        return HasPixelShaderPath();
    case AppearanceSettingKey::_PixelShaderImagePath:
        return HasPixelShaderImagePath();
    case AppearanceSettingKey::_BackgroundImagePath:
        return HasBackgroundImagePath();
    default:
        return false;
    }
}

void AppearanceConfig::ClearSetting(AppearanceSettingKey key)
{
    switch (key)
    {
#define _APPEARANCE_CLEAR_SETTING(type, name, jsonKey, ...) \
    case AppearanceSettingKey::name:                        \
        Clear##name();                                      \
        break;
        MTSM_APPEARANCE_SETTINGS(_APPEARANCE_CLEAR_SETTING)
#undef _APPEARANCE_CLEAR_SETTING
    case AppearanceSettingKey::_Foreground:
        ClearForeground();
        break;
    case AppearanceSettingKey::_Background:
        ClearBackground();
        break;
    case AppearanceSettingKey::_SelectionBackground:
        ClearSelectionBackground();
        break;
    case AppearanceSettingKey::_CursorColor:
        ClearCursorColor();
        break;
    case AppearanceSettingKey::_Opacity:
        ClearOpacity();
        break;
    case AppearanceSettingKey::_DarkColorSchemeName:
        ClearDarkColorSchemeName();
        break;
    case AppearanceSettingKey::_LightColorSchemeName:
        ClearLightColorSchemeName();
        break;
    case AppearanceSettingKey::_PixelShaderPath:
        ClearPixelShaderPath();
        break;
    case AppearanceSettingKey::_PixelShaderImagePath:
        ClearPixelShaderImagePath();
        break;
    case AppearanceSettingKey::_BackgroundImagePath:
        ClearBackgroundImagePath();
        break;
    default:
        break;
    }
}

std::vector<AppearanceSettingKey> AppearanceConfig::CurrentSettings() const
{
    std::vector<AppearanceSettingKey> result;
    for (auto i = 0; i < static_cast<int>(AppearanceSettingKey::SETTINGS_SIZE); i++)
    {
        const auto key = static_cast<AppearanceSettingKey>(i);
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
// - This is similar to Profile::LayerJson but for AppearanceConfig
// Arguments:
// - json: an object which should be a partial serialization of an AppearanceConfig object.
void AppearanceConfig::LayerJson(const Json::Value& json)
{
    // Merge incoming JSON keys into stored _json (key-wise, not replacement).
    // AppearanceConfig receives the full profile JSON; we store all keys and
    // read only appearance-relevant ones from it.
    JsonUtils::MergeJsonKeys(json, _json);

    // Nullable color settings are now JSON-backed. Log which were set.
    _logSettingIfSet(ForegroundKey, HasForeground());
    _logSettingIfSet(BackgroundKey, HasBackground());
    _logSettingIfSet(SelectionBackgroundKey, HasSelectionBackground());
    _logSettingIfSet(CursorColorKey, HasCursorColor());

    // Normalize legacy opacity key into canonical _json key
    if (json.isMember(JsonKey(LegacyAcrylicTransparencyKey)))
    {
        _json[JsonKey(OpacityKey)] = json[JsonKey(LegacyAcrylicTransparencyKey)];
    }
    // Normalize integer percent to float (e.g. 50 → 0.5)
    if (_json.isMember(JsonKey(OpacityKey)) && _json[JsonKey(OpacityKey)].isInt())
    {
        _json[JsonKey(OpacityKey)] = _json[JsonKey(OpacityKey)].asInt() / 100.0f;
    }
    _logSettingIfSet(OpacityKey, HasOpacity());

    // ColorScheme: translate the polymorphic on-disk "colorScheme" key (string, or
    // { "dark", "light" }) into the two independent internal keys. A string sets both sides;
    // an object sets only the side(s) present (the missing side is left untouched so it
    // inherits / retains a prior layer's value). Matches main's LayerJson behavior.
    // ColorScheme: translate the polymorphic on-disk "colorScheme" key (string, or
    // { "dark", "light" }) into the two independent internal keys. A string sets both sides;
    // an object sets only the side(s) present (the missing side is left untouched so it
    // inherits / retains a prior layer's value). Matches main's LayerJson behavior.
    if (const auto& colorScheme{ json[JsonKey(ColorSchemeKey)] }; colorScheme.isString())
    {
        _json[JsonKey(DarkColorSchemeNameKey)] = colorScheme;
        _json[JsonKey(LightColorSchemeNameKey)] = colorScheme;
        _logSettingSet(ColorSchemeKey);
    }
    else if (colorScheme.isObject())
    {
        if (colorScheme.isMember("dark") && !colorScheme["dark"].isNull())
        {
            _json[JsonKey(DarkColorSchemeNameKey)] = colorScheme["dark"];
            _logSettingSet("colorScheme.dark");
        }
        if (colorScheme.isMember("light") && !colorScheme["light"].isNull())
        {
            _json[JsonKey(LightColorSchemeNameKey)] = colorScheme["light"];
            _logSettingSet("colorScheme.light");
        }
    }
    // The raw polymorphic key was copied verbatim by MergeJsonKeys above; drop it so only the
    // two internal keys remain as the source of truth (ToJson re-emits "colorScheme").
    _json.removeMember(JsonKey(ColorSchemeKey));

    // MTSM settings are now JSON-backed (no backing fields).
    // Values are already in _json from the merge step above.
    // We only need to log which settings were set in this layer.
#define APPEARANCE_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    _logSettingIfSet(jsonKey, json.isMember(jsonKey) && !json[jsonKey].isNull());

    MTSM_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_LAYER_JSON)
#undef APPEARANCE_SETTINGS_LAYER_JSON

    // Complex/mutable settings — backing fields populated from _json for resolution lifecycle.
    // _json is the source of truth for serialization; backing fields are for resolution lifecycle.
    JsonUtils::GetValueForKey(json, "experimental.pixelShaderPath", _PixelShaderPath);
    _logSettingIfSet("experimental.pixelShaderPath", _PixelShaderPath.has_value());
    JsonUtils::GetValueForKey(json, "experimental.pixelShaderImagePath", _PixelShaderImagePath);
    _logSettingIfSet("experimental.pixelShaderImagePath", _PixelShaderImagePath.has_value());
    JsonUtils::GetValueForKey(json, "backgroundImage", _BackgroundImagePath);
    _logSettingIfSet("backgroundImage", _BackgroundImagePath.has_value());

    _ValidateThisLayer();
}

void AppearanceConfig::_ValidateThisLayer() const
{
    MTSM_APPEARANCE_SETTINGS(MTSM_VALIDATE_SETTING)

    // Settings declared outside MTSM_APPEARANCE_SETTINGS that are still JSON-backed.
    std::ignore = _getForegroundFromThisLayer();
    std::ignore = _getBackgroundFromThisLayer();
    std::ignore = _getSelectionBackgroundFromThisLayer();
    std::ignore = _getCursorColorFromThisLayer();
    std::ignore = _getOpacityFromThisLayer();
    std::ignore = _getDarkColorSchemeNameFromThisLayer();
    std::ignore = _getLightColorSchemeNameFromThisLayer();
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
