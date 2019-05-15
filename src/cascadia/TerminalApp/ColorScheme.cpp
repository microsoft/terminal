// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "../../types/inc/Utils.hpp"

using namespace TerminalApp;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

static const std::wstring NAME_KEY{ L"name" };
static const std::wstring TABLE_KEY{ L"colors" };
static const std::wstring FOREGROUND_KEY{ L"foreground" };
static const std::wstring BACKGROUND_KEY{ L"background" };
static const std::array<std::wstring, 16> TABLE_COLORS =
{
    L"black",
    L"red",
    L"green",
    L"yellow",
    L"blue",
    L"purple",
    L"cyan",
    L"white",
    L"brightBlack",
    L"brightRed",
    L"brightGreen",
    L"brightYellow",
    L"brightBlue",
    L"brightPurple",
    L"brightCyan",
    L"brightWhite"
};

ColorScheme::ColorScheme() :
    _schemeName{ L"" },
    _table{  },
    _defaultForeground{ RGB(242, 242, 242) },
    _defaultBackground{ RGB(12, 12, 12) }
{

}

ColorScheme::ColorScheme(std::wstring name, COLORREF defaultFg, COLORREF defaultBg) :
    _schemeName{ name },
    _table{  },
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
JsonObject ColorScheme::ToJson() const
{
    winrt::Windows::Data::Json::JsonObject jsonObject;

    auto fg = JsonValue::CreateStringValue(Utils::ColorToHexString(_defaultForeground));
    auto bg = JsonValue::CreateStringValue(Utils::ColorToHexString(_defaultBackground));
    auto name = JsonValue::CreateStringValue(_schemeName);

    jsonObject.Insert(NAME_KEY, name);
    jsonObject.Insert(FOREGROUND_KEY, fg);
    jsonObject.Insert(BACKGROUND_KEY, bg);
 
    int i = 0;
    for (const auto& current : TABLE_COLORS)
    {
        auto& color = _table.at(i);
        auto s = JsonValue::CreateStringValue(Utils::ColorToHexString(color));

        jsonObject.Insert(current, s);
        i++;
    }

    return jsonObject;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - a new ColorScheme instance created from the values in `json`
ColorScheme ColorScheme::FromJson(winrt::Windows::Data::Json::JsonObject json)
{
    ColorScheme result{};

    if (json.HasKey(NAME_KEY))
    {
        result._schemeName = json.GetNamedString(NAME_KEY);
    }
    if (json.HasKey(FOREGROUND_KEY))
    {
        const auto fgString = json.GetNamedString(FOREGROUND_KEY);
        const auto color = Utils::ColorFromHexString(fgString.c_str());
        result._defaultForeground = color;
    }
    if (json.HasKey(BACKGROUND_KEY))
    {
        const auto bgString = json.GetNamedString(BACKGROUND_KEY);
        const auto color = Utils::ColorFromHexString(bgString.c_str());
        result._defaultBackground = color;
    }

    // Legacy Deserialization. Leave in place to allow forward compatibility
    if (json.HasKey(TABLE_KEY))
    {
        const auto table = json.GetNamedArray(TABLE_KEY);
        int i = 0;

        for (auto v : table)
        {
            if (v.ValueType() == JsonValueType::String)
            {
                auto str = v.GetString();
                auto color = Utils::ColorFromHexString(str.c_str());
                result._table.at(i) = color;
            }
            i++;
        }
    }

    int i = 0;
    for (const auto& current : TABLE_COLORS)
    {
        if (json.HasKey(current))
        {
            const auto str = json.GetNamedString(current);
            const auto color = Utils::ColorFromHexString(str.c_str());
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
