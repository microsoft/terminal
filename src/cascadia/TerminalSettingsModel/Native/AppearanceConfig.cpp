// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../NativeSerialization.h"
#include "AppearanceConfig.h"
#include "PropertyDefinitions.h"
#include "MediaResourceResolver.h"
#include <DefaultSettings.h>

namespace Microsoft::Terminal::Settings::Model::Native
{
    static constexpr std::string_view ForegroundKey{ "foreground" };
    static constexpr std::string_view BackgroundKey{ "background" };
    static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
    static constexpr std::string_view CursorColorKey{ "cursorColor" };
    static constexpr std::string_view LegacyAcrylicTransparencyKey{ "acrylicOpacity" };
    static constexpr std::string_view OpacityKey{ "opacity" };
    static constexpr std::string_view ColorSchemeKey{ "colorScheme" };

    TSM_NATIVE_LIFETIME_DEFINITIONS(AppearanceConfig)
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, std::optional<Native::Color>, Foreground, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, std::optional<Native::Color>, Background, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, std::optional<Native::Color>, SelectionBackground, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, std::optional<Native::Color>, CursorColor, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, float, Opacity, false, "", 1.0f)
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, winrt::hstring, DarkColorSchemeName, false, "", L"Campbell")
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, winrt::hstring, LightColorSchemeName, false, "", L"Campbell")

#define DEFINE_NATIVE_APPEARANCE_PROPERTY(type, name, jsonKey, ...) \
    TSM_NATIVE_PROPERTY_DEFINITIONS(AppearanceConfig, type, name, false, jsonKey, __VA_ARGS__)
    NATIVE_APPEARANCE_SETTINGS(DEFINE_NATIVE_APPEARANCE_PROPERTY)
#undef DEFINE_NATIVE_APPEARANCE_PROPERTY

    AppearanceConfig::AppearanceConfig(ConstructionToken, winrt::weak_ref<Profile> sourceProfile) :
        _sourceProfile{ std::move(sourceProfile) }
    {
    }

    AppearanceConfig::~AppearanceConfig() noexcept = default;

    winrt::com_ptr<AppearanceConfig> AppearanceConfig::Create(winrt::weak_ref<Profile> sourceProfile)
    {
        return winrt::make_self<AppearanceConfig>(ConstructionToken{}, std::move(sourceProfile));
    }

    void AppearanceConfig::ClearParents()
    {
        _parents.clear();
    }

    void AppearanceConfig::AddLeastImportantParent(winrt::com_ptr<AppearanceConfig> parent)
    {
        _parents.emplace_back(std::move(parent));
    }

    void AppearanceConfig::AddMostImportantParent(winrt::com_ptr<AppearanceConfig> parent)
    {
        _parents.emplace(_parents.begin(), std::move(parent));
    }

    std::span<const winrt::com_ptr<AppearanceConfig>> AppearanceConfig::Parents() const noexcept
    {
        return _parents;
    }

    winrt::com_ptr<AppearanceConfig> AppearanceConfig::CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Profile> sourceProfile)
    {
        auto appearance = Create(std::move(sourceProfile));
        appearance->_Foreground = source->_Foreground;
        appearance->_Background = source->_Background;
        appearance->_SelectionBackground = source->_SelectionBackground;
        appearance->_CursorColor = source->_CursorColor;
        appearance->_Opacity = source->_Opacity;
        appearance->_DarkColorSchemeName = source->_DarkColorSchemeName;
        appearance->_LightColorSchemeName = source->_LightColorSchemeName;

#define COPY_NATIVE_APPEARANCE_PROPERTY(type, name, ...) appearance->_##name = source->_##name;
        NATIVE_APPEARANCE_SETTINGS(COPY_NATIVE_APPEARANCE_PROPERTY)
#undef COPY_NATIVE_APPEARANCE_PROPERTY
        return appearance;
    }

    Json::Value AppearanceConfig::ToJson() const
    {
        Json::Value json{ Json::objectValue };
        JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
        JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
        JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
        JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);
        JsonUtils::SetValueForKey(json, OpacityKey, _Opacity, JsonUtils::OptionalConverter<float, IntAsFloatPercentConversionTrait>{});
        if (HasDarkColorSchemeName() || HasLightColorSchemeName())
        {
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
#define SERIALIZE_NATIVE_APPEARANCE_PROPERTY(type, name, jsonKey, ...) JsonUtils::SetValueForKey(json, jsonKey, _##name);
        NATIVE_APPEARANCE_SETTINGS(SERIALIZE_NATIVE_APPEARANCE_PROPERTY)
#undef SERIALIZE_NATIVE_APPEARANCE_PROPERTY
        return json;
    }

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
            JsonUtils::GetValueForKey(json, ColorSchemeKey, _DarkColorSchemeName);
            _LightColorSchemeName = _DarkColorSchemeName;
            _logSettingSet(ColorSchemeKey);
        }
        else if (json["colorScheme"].isObject())
        {
            JsonUtils::GetValueForKey(json["colorScheme"], "dark", _DarkColorSchemeName);
            JsonUtils::GetValueForKey(json["colorScheme"], "light", _LightColorSchemeName);
            _logSettingSet("colorScheme.dark");
            _logSettingSet("colorScheme.light");
        }

#define PARSE_NATIVE_APPEARANCE_PROPERTY(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);             \
    _logSettingIfSet(jsonKey, _##name.has_value());
        NATIVE_APPEARANCE_SETTINGS(PARSE_NATIVE_APPEARANCE_PROPERTY)
#undef PARSE_NATIVE_APPEARANCE_PROPERTY
    }

    winrt::com_ptr<Profile> AppearanceConfig::SourceProfile() const
    {
        return _sourceProfile.get();
    }

    std::tuple<winrt::hstring, OriginTag> AppearanceConfig::_getSourceProfileBasePathAndOrigin() const
    {
        if (const auto profile = _sourceProfile.get())
        {
            return { profile->SourceBasePath(), profile->Origin() };
        }
        return { winrt::hstring{}, OriginTag::None };
    }

    void AppearanceConfig::ResolveMediaResources(const MediaResourceResolver& resolver)
    {
        const auto resolve = [&](const auto& sourceAndValue) {
            const auto& [source, resource] = sourceAndValue;
            if (source && resource && *resource)
            {
                const auto [basePath, origin] = source->_getSourceProfileBasePathAndOrigin();
                ResolveMediaResource(origin, basePath, *resource, resolver);
            }
        };
        resolve(_getBackgroundImagePathOverrideSourceAndValueImpl());
        resolve(_getPixelShaderPathOverrideSourceAndValueImpl());
        resolve(_getPixelShaderImagePathOverrideSourceAndValueImpl());
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
}
