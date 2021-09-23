// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
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

ColorScheme::ColorScheme() noexcept :
    ColorScheme{ winrt::hstring{} }
{
}

ColorScheme::ColorScheme(const winrt::hstring& name) noexcept :
    _Name{ name }
{
    const auto table = Utils::CampbellColorTable();
    std::copy_n(table.data(), table.size(), _table.data());
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
// - Returns nullptr for invalid JSON.
winrt::com_ptr<ColorScheme> ColorScheme::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<ColorScheme>(uninitialized_t{});
    return result->_layerJson(json) ? result : nullptr;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// Arguments:
// - json: an object which should be a full serialization of a ColorScheme object.
// Return Value:
// - Returns true if the given JSON was valid.
bool ColorScheme::_layerJson(const Json::Value& json)
{
    // Required fields
    auto isValid = JsonUtils::GetValueForKey(json, NameKey, _Name);

    // Optional fields (they have defaults in ColorScheme.h)
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);

    // Required fields
    for (unsigned int i = 0; i < TableColors.size(); ++i)
    {
        isValid &= JsonUtils::GetValueForKey(json, til::at(TableColors, i), til::at(_table, i));
    }

    return isValid;
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
        JsonUtils::SetValueForKey(json, til::at(TableColors, i), til::at(_table, i));
    }

    return json;
}

winrt::com_array<winrt::Microsoft::Terminal::Core::Color> ColorScheme::Table() const noexcept
{
    return winrt::com_array<Core::Color>{ _table };
}

// Method Description:
// - Set a color in the color table
// Arguments:
// - index: the index of the desired color within the table
// - value: the color value we are setting the color table color to
// Return Value:
// - none
void ColorScheme::SetColorTableEntry(uint8_t index, const Core::Color& value) noexcept
{
    THROW_HR_IF(E_INVALIDARG, index >= _table.size());
    _table[index] = value;
}
