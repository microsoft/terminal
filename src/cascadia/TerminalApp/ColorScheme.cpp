// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "../../types/inc/Utils.hpp"
#include "Utils.h"

using namespace TerminalApp;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view TableKey{ "colors" };
static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
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
    _defaultForeground{ RGB(242, 242, 242) },
    _defaultBackground{ RGB(12, 12, 12) }
{
}

ColorScheme::ColorScheme(std::wstring name, COLORREF defaultFg, COLORREF defaultBg) :
    _schemeName{ name },
    _table{},
    _defaultForeground{ defaultFg },
    _defaultBackground{ defaultBg }
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

    for (int i = 0; i < _table.size(); i++)
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
    ColorScheme result{};

    if (auto name{ json[JsonKey(NameKey)] })
    {
        result._schemeName = winrt::to_hstring(name.asString());
    }
    if (auto fgString{ json[JsonKey(ForegroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(fgString.asString());
        result._defaultForeground = color;
    }
    if (auto bgString{ json[JsonKey(BackgroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(bgString.asString());
        result._defaultBackground = color;
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
                result._table.at(i) = color;
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
            result._table.at(i) = color;
        }
        i++;
    }

    return result;
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
