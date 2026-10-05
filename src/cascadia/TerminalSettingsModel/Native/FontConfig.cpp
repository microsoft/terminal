// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../NativeSerialization.h"
#include "FontConfig.h"
#include "PropertyDefinitions.h"
#include <DefaultSettings.h>

namespace Microsoft::Terminal::Settings::Model::Native
{
    static constexpr std::string_view FontInfoKey{ "font" };
    static constexpr std::string_view LegacyFontFaceKey{ "fontFace" };
    static constexpr std::string_view LegacyFontSizeKey{ "fontSize" };
    static constexpr std::string_view LegacyFontWeightKey{ "fontWeight" };

    TSM_NATIVE_LIFETIME_DEFINITIONS(FontConfig)

#define DEFINE_NATIVE_FONT_PROPERTY(type, name, jsonKey, ...) \
    TSM_NATIVE_PROPERTY_DEFINITIONS(FontConfig, type, name, false, jsonKey, __VA_ARGS__)
    NATIVE_FONT_SETTINGS(DEFINE_NATIVE_FONT_PROPERTY)
#undef DEFINE_NATIVE_FONT_PROPERTY

    FontConfig::FontConfig(ConstructionToken, winrt::weak_ref<Profile> sourceProfile) :
        _sourceProfile{ std::move(sourceProfile) }
    {
    }

    FontConfig::~FontConfig() noexcept = default;

    winrt::com_ptr<FontConfig> FontConfig::Create(winrt::weak_ref<Profile> sourceProfile)
    {
        return winrt::make_self<FontConfig>(ConstructionToken{}, std::move(sourceProfile));
    }

    void FontConfig::ClearParents()
    {
        _parents.clear();
    }

    void FontConfig::AddLeastImportantParent(winrt::com_ptr<FontConfig> parent)
    {
        _parents.emplace_back(std::move(parent));
    }

    void FontConfig::AddMostImportantParent(winrt::com_ptr<FontConfig> parent)
    {
        _parents.emplace(_parents.begin(), std::move(parent));
    }

    std::span<const winrt::com_ptr<FontConfig>> FontConfig::Parents() const noexcept
    {
        return _parents;
    }

    winrt::com_ptr<FontConfig> FontConfig::CopyFontInfo(const FontConfig* source, winrt::weak_ref<Profile> sourceProfile)
    {
        auto fontInfo = Create(std::move(sourceProfile));

#define COPY_NATIVE_FONT_PROPERTY(type, name, ...) fontInfo->_##name = source->_##name;
        NATIVE_FONT_SETTINGS(COPY_NATIVE_FONT_PROPERTY)
#undef COPY_NATIVE_FONT_PROPERTY

        const auto cloneFontMap = [](const FontAxesMap& map) {
            std::map<winrt::hstring, float> values;
            for (const auto& [key, value] : map)
            {
                values.emplace(key, value);
            }
            return winrt::single_threaded_map(std::move(values));
        };
        if (source->_FontAxes && *source->_FontAxes)
        {
            fontInfo->_FontAxes = cloneFontMap(*source->_FontAxes);
        }
        if (source->_FontFeatures && *source->_FontFeatures)
        {
            fontInfo->_FontFeatures = cloneFontMap(*source->_FontFeatures);
        }
        return fontInfo;
    }

    Json::Value FontConfig::ToJson() const
    {
        Json::Value json{ Json::objectValue };
#define SERIALIZE_NATIVE_FONT_PROPERTY(type, name, jsonKey, ...) JsonUtils::SetValueForKey(json, jsonKey, _##name);
        NATIVE_FONT_SETTINGS(SERIALIZE_NATIVE_FONT_PROPERTY)
#undef SERIALIZE_NATIVE_FONT_PROPERTY
        return json;
    }

    void FontConfig::LayerJson(const Json::Value& json)
    {
        if (json.isMember(JsonKey(FontInfoKey)))
        {
            const auto fontInfoJson = json[JsonKey(FontInfoKey)];
#define PARSE_NATIVE_FONT_PROPERTY(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(fontInfoJson, jsonKey, _##name); \
    _logSettingIfSet(jsonKey, _##name.has_value());
            NATIVE_FONT_SETTINGS(PARSE_NATIVE_FONT_PROPERTY)
#undef PARSE_NATIVE_FONT_PROPERTY
        }
        else
        {
            JsonUtils::GetValueForKey(json, LegacyFontFaceKey, _FontFace);
            _logSettingIfSet("face", _FontFace.has_value());
            JsonUtils::GetValueForKey(json, LegacyFontSizeKey, _FontSize);
            _logSettingIfSet("size", _FontSize.has_value());
            JsonUtils::GetValueForKey(json, LegacyFontWeightKey, _FontWeight);
            _logSettingIfSet("weight", _FontWeight.has_value());
        }
    }

    winrt::com_ptr<Profile> FontConfig::SourceProfile() const
    {
        return _sourceProfile.get();
    }

    void FontConfig::_logSettingSet(const std::string_view& setting)
    {
        if (setting == "axes" && _FontAxes.has_value())
        {
            for (const auto& [key, value] : _FontAxes.value())
            {
                _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, til::u16u8(key)));
            }
        }
        else if (setting == "features" && _FontFeatures.has_value())
        {
            for (const auto& [key, value] : _FontFeatures.value())
            {
                _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, til::u16u8(key)));
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
}
