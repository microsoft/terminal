// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Theme.h"
#include "../../types/inc/Utils.hpp"
#include "../../types/inc/colorTable.hpp"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "ThemeColor.g.cpp"
#include "WindowTheme.g.cpp"
#include "TabRowTheme.g.cpp"
#include "Theme.g.cpp"

using namespace ::Microsoft::Console;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI;

static constexpr std::string_view NameKey{ "name" };

winrt::Microsoft::Terminal::Settings::Model::ThemeColor ThemeColor::FromColor(const winrt::Microsoft::Terminal::Core::Color& coreColor) noexcept
{
    auto result = winrt::make_self<implementation::ThemeColor>();
    result->_Color = coreColor;
    result->_ColorType = ThemeColorType::Color;
    return *result;
}

winrt::Microsoft::Terminal::Settings::Model::ThemeColor ThemeColor::FromAccent() noexcept
{
    auto result = winrt::make_self<implementation::ThemeColor>();
    result->_ColorType = ThemeColorType::Accent;
    return *result;
}

#define THEME_SETTINGS_FROM_JSON(type, name, jsonKey, ...) \
    result->name(JsonUtils::GetValueForKey<type>(json, jsonKey));

#define THEME_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, val.name());

#define THEME_OBJECT_CONVERTER(nameSpace, name, macro)                                         \
    template<>                                                                                 \
    struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<nameSpace::name> \
    {                                                                                          \
        nameSpace::name FromJson(const Json::Value& json)                                      \
        {                                                                                      \
            auto result = winrt::make_self<nameSpace::implementation::name>();                 \
            macro(THEME_SETTINGS_FROM_JSON) return *result;                                    \
        }                                                                                      \
                                                                                               \
        bool CanConvert(const Json::Value& json)                                               \
        {                                                                                      \
            return json.isObject();                                                            \
        }                                                                                      \
                                                                                               \
        Json::Value ToJson(const nameSpace::name& val)                                         \
        {                                                                                      \
            Json::Value json{ Json::ValueType::objectValue };                                  \
            macro(THEME_SETTINGS_TO_JSON) return json;                                         \
        }                                                                                      \
                                                                                               \
        std::string TypeDescription() const                                                    \
        {                                                                                      \
            return "name (You should never see this)";                                         \
        }                                                                                      \
    };

THEME_OBJECT_CONVERTER(winrt::Microsoft::Terminal::Settings::Model, WindowTheme, MTSM_THEME_WINDOW_SETTINGS);
THEME_OBJECT_CONVERTER(winrt::Microsoft::Terminal::Settings::Model, TabRowTheme, MTSM_THEME_TABROW_SETTINGS);

#undef THEME_SETTINGS_FROM_JSON
#undef THEME_SETTINGS_TO_JSON
#undef THEME_OBJECT_CONVERTER

Theme::Theme() noexcept :
    Theme{ winrt::Windows::UI::Xaml::ElementTheme::Default }
{
}

Theme::Theme(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept
{
    auto window{ winrt::make_self<implementation::WindowTheme>() };
    window->RequestedTheme(requestedTheme);
    _Window = *window;
}

winrt::com_ptr<Theme> Theme::Copy() const
{
    auto theme{ winrt::make_self<Theme>() };

#define THEME_SETTINGS_COPY(type, name, jsonKey, ...) \
    theme->_##name = _##name;

    MTSM_THEME_SETTINGS(THEME_SETTINGS_COPY)
#undef THEME_SETTINGS_COPY

    return theme;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - Returns nullptr for invalid JSON.
winrt::com_ptr<Theme> Theme::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<Theme>();
    result->LayerJson(json);
    return result;
}

void Theme::LayerJson(const Json::Value& json)
{
    if (json.isString())
    {
        // We found a string, not an object. Just secretly promote that string
        // to a theme object with just the applicationTheme set from that value.
        JsonUtils::GetValue(json, _Name);
        winrt::Windows::UI::Xaml::ElementTheme requestedTheme{ winrt::Windows::UI::Xaml::ElementTheme::Default };
        JsonUtils::GetValue(json, requestedTheme);

        auto window{ winrt::make_self<implementation::WindowTheme>() };
        window->RequestedTheme(requestedTheme);
        _Window = *window;

        return;
    }

    JsonUtils::GetValueForKey(json, NameKey, _Name);

    // This will use each of the ConversionTrait's from above to quickly parse the sub-objects

#define THEME_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);

    MTSM_THEME_SETTINGS(THEME_SETTINGS_LAYER_JSON)
#undef THEME_SETTINGS_LAYER_JSON
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value Theme::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, NameKey, _Name);

#define THEME_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);

    MTSM_THEME_SETTINGS(THEME_SETTINGS_TO_JSON)
#undef THEME_SETTINGS_TO_JSON

    return json;
}
