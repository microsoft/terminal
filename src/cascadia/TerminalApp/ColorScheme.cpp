// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "DefaultSettings.h"
#include "../../types/inc/Utils.hpp"
#include "Utils.h"
#include "JsonUtils.h"

using namespace ::Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view TableKey{ "colors" };
static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
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
    _schemeName{ L"" },
    _table{},
    _defaultForeground{ DEFAULT_FOREGROUND_WITH_ALPHA },
    _defaultBackground{ DEFAULT_BACKGROUND_WITH_ALPHA },
    _selectionBackground{ DEFAULT_FOREGROUND }
{
}

ColorScheme::ColorScheme(std::wstring name, COLORREF defaultFg, COLORREF defaultBg) :
    _schemeName{ name },
    _table{},
    _defaultForeground{ defaultFg },
    _defaultBackground{ defaultBg },
    _selectionBackground{ DEFAULT_FOREGROUND }
{
}

ColorScheme::~ColorScheme()
{
}

// Method Description:
// - Apply our values to the given TerminalSettings object. Sets the foreground,
//      background, and color table of the settings object.
// Arguments:
// - terminalSettings: the object to apply our settings to.
// Return Value:
// - <none>
void ColorScheme::ApplyScheme(TerminalSettings terminalSettings) const
{
    terminalSettings.DefaultForeground(_defaultForeground);
    terminalSettings.DefaultBackground(_defaultBackground);
    terminalSettings.SelectionBackground(_selectionBackground);

    auto const tableCount = gsl::narrow_cast<int>(_table.size());
    for (int i = 0; i < tableCount; i++)
    {
        terminalSettings.SetColorTableEntry(i, _table[i]);
    }
}

// Method Description:
// - Serialize this object to a JsonObject.
// Arguments:
// - <none>
// Return Value:
// - a JsonObject which is an equivalent serialization of this object.
Json::Value ColorScheme::ToJson() const
{
    Json::Value root;
    root[JsonKey(NameKey)] = winrt::to_string(_schemeName);
    root[JsonKey(ForegroundKey)] = Utils::ColorToHexString(_defaultForeground);
    root[JsonKey(BackgroundKey)] = Utils::ColorToHexString(_defaultBackground);
    root[JsonKey(SelectionBackgroundKey)] = Utils::ColorToHexString(_selectionBackground);

    int i = 0;
    for (const auto& colorName : TableColors)
    {
        auto& colorValue = _table.at(i);
        root[JsonKey(colorName)] = Utils::ColorToHexString(colorValue);
        i++;
    }

    return root;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - a new ColorScheme instance created from the values in `json`
ColorScheme ColorScheme::FromJson(const Json::Value& json)
{
    ColorScheme result;
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
bool ColorScheme::ShouldBeLayered(const Json::Value& json) const
{
    if (const auto name{ json[JsonKey(NameKey)] })
    {
        const auto nameFromJson = GetWstringFromJson(name);
        return nameFromJson == _schemeName;
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
    if (auto name{ json[JsonKey(NameKey)] })
    {
        _schemeName = winrt::to_hstring(name.asString());
    }
    if (auto fgString{ json[JsonKey(ForegroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(fgString.asString());
        _defaultForeground = color;
    }
    if (auto bgString{ json[JsonKey(BackgroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(bgString.asString());
        _defaultBackground = color;
    }
    if (auto sbString{ json[JsonKey(SelectionBackgroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(sbString.asString());
        _selectionBackground = color;
    }

    // Legacy Deserialization. Leave in place to allow forward compatibility
    if (auto table{ json[JsonKey(TableKey)] })
    {
        int i = 0;

        for (const auto& tableEntry : table)
        {
            if (tableEntry.isString())
            {
                auto color = Utils::ColorFromHexString(tableEntry.asString());
                _table.at(i) = color;
            }
            i++;
        }
    }

    int i = 0;
    for (const auto& current : TableColors)
    {
        if (auto str{ json[JsonKey(current)] })
        {
            const auto color = Utils::ColorFromHexString(str.asString());
            _table.at(i) = color;
        }
        i++;
    }
}

std::wstring_view ColorScheme::GetName() const noexcept
{
    return { _schemeName };
}

std::array<COLORREF, COLOR_TABLE_SIZE>& ColorScheme::GetTable() noexcept
{
    return _table;
}

COLORREF ColorScheme::GetForeground() const noexcept
{
    return _defaultForeground;
}

COLORREF ColorScheme::GetBackground() const noexcept
{
    return _defaultBackground;
}

COLORREF ColorScheme::GetSelectionBackground() const noexcept
{
    return _selectionBackground;
}

// Method Description:
// - Parse the name from the JSON representation of a ColorScheme.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - the name of the color scheme represented by `json` as a std::wstring optional
//   i.e. the value of the `name` property.
// - returns std::nullopt if `json` doesn't have the `name` property
std::optional<std::wstring> TerminalApp::ColorScheme::GetNameFromJson(const Json::Value& json)
{
    if (const auto name{ json[JsonKey(NameKey)] })
    {
        return GetWstringFromJson(name);
    }
    return std::nullopt;
}
