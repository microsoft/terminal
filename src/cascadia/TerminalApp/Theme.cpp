// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Theme.h"
#include "DefaultSettings.h"
#include "../../types/inc/Utils.hpp"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

using namespace ::Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view WindowApplicationThemeKey{ "window.applicationTheme" };
// static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view TabRowBackgroundKey{ "tabRow.background" };

Theme::Theme()
{
}

Theme::~Theme()
{
}

// Method Description:
// - Apply our values to the given TerminalSettings object. Sets the foreground,
//      background, and color table of the settings object.
// Arguments:
// - terminalSettings: the object to apply our settings to.
// Return Value:
// - <none>
void Theme::ApplyTheme(TerminalSettings terminalSettings) const
{
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Theme object.
// Return Value:
// - a new Theme instance created from the values in `json`
Theme Theme::FromJson(const Json::Value& json)
{
    Theme result;
    result.LayerJson(json);
    return result;
}

// Method Description:
// - Returns true if we think the provided json object represents an instance of
//   the same object as this object. If true, we should layer that json object
//   on us, instead of creating a new object.
// Arguments:
// - json: The json object to query to see if it's the same
// Return Value:
// - true iff the json object has the same `name` as we do.
bool Theme::ShouldBeLayered(const Json::Value& json) const
{
    std::wstring nameFromJson{};
    if (JsonUtils::GetValueForKey(json, NameKey, nameFromJson))
    {
        return nameFromJson == _Name;
    }
    return false;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// Arguments:
// - json: an object which should be a partial serialization of a Theme object.
// Return Value:
// <none>
void Theme::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, NameKey, _Name);
    JsonUtils::GetValueForKey(json, WindowApplicationThemeKey, _ApplicationTheme);
    // JsonUtils::GetValueForKey(json, ForegroundKey, _defaultForeground);
    JsonUtils::GetValueForKey(json, TabRowBackgroundKey, _TabRowBackground);
}

// Method Description:
// - Parse the name from the JSON representation of a Theme.
// Arguments:
// - json: an object which should be a serialization of a Theme object.
// Return Value:
// - the name of the color scheme represented by `json` as a std::wstring optional
//   i.e. the value of the `name` property.
// - returns std::nullopt if `json` doesn't have the `name` property
std::optional<std::wstring> Theme::GetNameFromJson(const Json::Value& json)
{
    return JsonUtils::GetValueForKey<std::optional<std::wstring>>(json, NameKey);
}
