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

static constexpr size_t ColorSchemeExpectedSize = 16;
static constexpr std::array<std::pair<std::string_view, size_t>, 18> TableColorsMapping{ {
    // Primary color mappings
    { "black", 0 },
    { "red", 1 },
    { "green", 2 },
    { "yellow", 3 },
    { "blue", 4 },
    { "purple", 5 },
    { "cyan", 6 },
    { "white", 7 },
    { "brightBlack", 8 },
    { "brightRed", 9 },
    { "brightGreen", 10 },
    { "brightYellow", 11 },
    { "brightBlue", 12 },
    { "brightPurple", 13 },
    { "brightCyan", 14 },
    { "brightWhite", 15 },

    // Alternate color mappings (GH#11456)
    { "magenta", 5 },
    { "brightMagenta", 13 },
} };

ColorScheme::ColorScheme() noexcept :
    ColorScheme{ winrt::hstring{} }
{
}

ColorScheme::ColorScheme(const winrt::hstring& name) noexcept :
    _Name{ name },
    _Origin{ OriginTag::User }
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
    scheme->_Origin = _Origin;
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
    size_t colorCount = 0;
    for (const auto& [key, index] : TableColorsMapping)
    {
        colorCount += JsonUtils::GetValueForKey(json, key, til::at(_table, index));
        if (colorCount == ColorSchemeExpectedSize)
        {
            break;
        }
    }

    isValid &= (colorCount == 16); // Valid schemes should have exactly 16 colors

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

    for (size_t i = 0; i < ColorSchemeExpectedSize; ++i)
    {
        const auto& key = til::at(TableColorsMapping, i).first;
        JsonUtils::SetValueForKey(json, key, til::at(_table, i));
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

winrt::Microsoft::Terminal::Core::Scheme ColorScheme::ToCoreScheme() const noexcept
{
    winrt::Microsoft::Terminal::Core::Scheme coreScheme{};

    coreScheme.Foreground = Foreground();
    coreScheme.Background = Background();
    coreScheme.CursorColor = CursorColor();
    coreScheme.SelectionBackground = SelectionBackground();
    coreScheme.Black = Table()[0];
    coreScheme.Red = Table()[1];
    coreScheme.Green = Table()[2];
    coreScheme.Yellow = Table()[3];
    coreScheme.Blue = Table()[4];
    coreScheme.Purple = Table()[5];
    coreScheme.Cyan = Table()[6];
    coreScheme.White = Table()[7];
    coreScheme.BrightBlack = Table()[8];
    coreScheme.BrightRed = Table()[9];
    coreScheme.BrightGreen = Table()[10];
    coreScheme.BrightYellow = Table()[11];
    coreScheme.BrightBlue = Table()[12];
    coreScheme.BrightPurple = Table()[13];
    coreScheme.BrightCyan = Table()[14];
    coreScheme.BrightWhite = Table()[15];
    return coreScheme;
}

bool ColorScheme::IsEquivalentForSettingsMergePurposes(const winrt::com_ptr<ColorScheme>& other) noexcept
{
    // The caller likely only got here if the names were the same, so skip checking that one.
    // We do not care about the cursor color or the selection background, as the main reason we are
    // doing equivalence merging is to replace old, poorly-specified versions of those two properties.
    return _table == other->_table && _Background == other->_Background && _Foreground == other->_Foreground;
}
