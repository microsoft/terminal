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
    _schemeName{ L"" },
    _table{},
    _defaultForeground{ DEFAULT_FOREGROUND_WITH_ALPHA },
    _defaultBackground{ DEFAULT_BACKGROUND_WITH_ALPHA },
    _selectionBackground{ DEFAULT_FOREGROUND },
    _cursorColor{ DEFAULT_CURSOR_COLOR }
{
}

ColorScheme::ColorScheme(std::wstring name, til::color defaultFg, til::color defaultBg, til::color cursorColor) :
    _schemeName{ name },
    _table{},
    _defaultForeground{ defaultFg },
    _defaultBackground{ defaultBg },
    _selectionBackground{ DEFAULT_FOREGROUND },
    _cursorColor{ cursorColor }
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
    terminalSettings.DefaultForeground(static_cast<COLORREF>(_defaultForeground));
    terminalSettings.DefaultBackground(static_cast<COLORREF>(_defaultBackground));
    terminalSettings.SelectionBackground(static_cast<COLORREF>(_selectionBackground));
    terminalSettings.CursorColor(static_cast<COLORREF>(_cursorColor));

    auto const tableCount = gsl::narrow_cast<int>(_table.size());
    for (int i = 0; i < tableCount; i++)
    {
        terminalSettings.SetColorTableEntry(i, static_cast<COLORREF>(_table[i]));
    }
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
    if (auto sbString{ json[JsonKey(CursorColorKey)] })
    {
        const auto color = Utils::ColorFromHexString(sbString.asString());
        _cursorColor = color;
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

std::array<til::color, COLOR_TABLE_SIZE>& ColorScheme::GetTable() noexcept
{
    return _table;
}

til::color ColorScheme::GetForeground() const noexcept
{
    return _defaultForeground;
}

til::color ColorScheme::GetBackground() const noexcept
{
    return _defaultBackground;
}

til::color ColorScheme::GetSelectionBackground() const noexcept
{
    return _selectionBackground;
}

til::color ColorScheme::GetCursorColor() const noexcept
{
    return _cursorColor;
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
