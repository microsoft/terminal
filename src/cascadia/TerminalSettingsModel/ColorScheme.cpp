// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "DefaultSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../types/inc/colorTable.hpp"
#include "Utils.h"
#include "JsonUtils.h"

#include "ColorScheme.g.cpp"

using namespace ::Microsoft::Console;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };

static constexpr std::array<std::string_view, 16> TableColors = {
    "black",
    "red",
    "green",
    "yellow",
    "blue",
    "purple",
    "cyan",
    "white",
    "brightBlack",
    "brightRed",
    "brightGreen",
    "brightYellow",
    "brightBlue",
    "brightPurple",
    "brightCyan",
    "brightWhite"
};

ColorScheme::ColorScheme() :
    ColorScheme(L"", DEFAULT_FOREGROUND, DEFAULT_BACKGROUND, DEFAULT_CURSOR_COLOR)
{
    Utils::InitializeCampbellColorTable(_table);
}

ColorScheme::ColorScheme(winrt::hstring name) :
    ColorScheme(name, DEFAULT_FOREGROUND, DEFAULT_BACKGROUND, DEFAULT_CURSOR_COLOR)
{
    Utils::InitializeCampbellColorTable(_table);
}

ColorScheme::ColorScheme(winrt::hstring name, til::color defaultFg, til::color defaultBg, til::color cursorColor) :
    _Name{ name },
    _Foreground{ defaultFg },
    _Background{ defaultBg },
    _SelectionBackground{ DEFAULT_FOREGROUND },
    _CursorColor{ cursorColor }
{
}

winrt::com_ptr<ColorScheme> ColorScheme::Copy() const
{
    auto scheme{ winrt::make_self<ColorScheme>() };
    scheme->_Name = _Name;
    scheme->_Foreground = _Foreground;
    scheme->_Background = _Background;
    scheme->_SelectionBackground = _SelectionBackground;
    scheme->_CursorColor = _CursorColor;
    scheme->_table = _table;
    return scheme;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - a new ColorScheme instance created from the values in `json`
winrt::com_ptr<ColorScheme> ColorScheme::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<ColorScheme>();
    result->LayerJson(json);
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
bool ColorScheme::ShouldBeLayered(const Json::Value& json) const
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
// - json: an object which should be a partial serialization of a ColorScheme object.
// Return Value:
// <none>
void ColorScheme::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, NameKey, _Name);
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);

    for (unsigned int i = 0; i < TableColors.size(); ++i)
    {
        JsonUtils::GetValueForKey(json, til::at(TableColors, i), _table.at(i));
    }
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value ColorScheme::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);

    for (unsigned int i = 0; i < TableColors.size(); ++i)
    {
        JsonUtils::SetValueForKey(json, til::at(TableColors, i), _table.at(i));
    }

    return json;
}

winrt::com_array<winrt::Microsoft::Terminal::Core::Color> ColorScheme::Table() const noexcept
{
    winrt::com_array<winrt::Microsoft::Terminal::Core::Color> result{ base::checked_cast<uint32_t>(_table.size()) };
    std::transform(_table.begin(), _table.end(), result.begin(), [](til::color c) -> winrt::Microsoft::Terminal::Core::Color { return c; });
    return result;
}

// Method Description:
// - Set a color in the color table
// Arguments:
// - index: the index of the desired color within the table
// - value: the color value we are setting the color table color to
// Return Value:
// - none
void ColorScheme::SetColorTableEntry(uint8_t index, const winrt::Microsoft::Terminal::Core::Color& value) noexcept
{
    THROW_HR_IF(E_INVALIDARG, index > _table.size() - 1);
    _table[index] = value;
}

// Method Description:
// - Validates a given color scheme
// - A color scheme is valid if it has a name and defines all the colors
// Arguments:
// - The color scheme to validate
// Return Value:
// - true if the scheme is valid, false otherwise
bool ColorScheme::ValidateColorScheme(const Json::Value& scheme)
{
    for (const auto& key : TableColors)
    {
        if (!scheme.isMember(JsonKey(key)))
        {
            return false;
        }
    }
    if (!scheme.isMember(JsonKey(NameKey)))
    {
        return false;
    }
    return true;
}

// Method Description:
// - Parse the name from the JSON representation of a ColorScheme.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - the name of the color scheme represented by `json` as a std::wstring optional
//   i.e. the value of the `name` property.
// - returns std::nullopt if `json` doesn't have the `name` property
std::optional<std::wstring> ColorScheme::GetNameFromJson(const Json::Value& json)
{
    return JsonUtils::GetValueForKey<std::optional<std::wstring>>(json, NameKey);
}
