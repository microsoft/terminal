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
#include "Theme.g.cpp"

using namespace ::Microsoft::Console;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI;

static constexpr std::string_view NameKey{ "name" };

Theme::Theme() noexcept :
    Theme{ winrt::Windows::UI::Xaml::ElementTheme::Default }
{
}

Theme::Theme(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept :
    _RequestedTheme{ requestedTheme }
{
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
        JsonUtils::GetValue(json, _Name);
        JsonUtils::GetValue(json, _RequestedTheme);
        return;
    }

    JsonUtils::GetValueForKey(json, NameKey, _Name);

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

ThemeColor::ThemeColor() noexcept
{
}

ThemeColor::ThemeColor(const winrt::Microsoft::Terminal::Core::Color& coreColor) noexcept :
    _Color{ coreColor }
{
}
