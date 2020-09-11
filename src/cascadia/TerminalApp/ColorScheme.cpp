// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "DefaultSettings.h"
#include "../../types/inc/Utils.hpp"
#include "Utils.h"
#include "JsonUtils.h"

#include "ColorScheme.g.cpp"

using namespace ::Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::TerminalApp::implementation;
using namespace winrt::Windows::UI;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };

static constexpr std::string_view BlackKey{ "black" };
static constexpr std::string_view RedKey{ "red" };
static constexpr std::string_view GreenKey{ "green" };
static constexpr std::string_view YellowKey{ "yellow" };
static constexpr std::string_view BlueKey{ "blue" };
static constexpr std::string_view PurpleKey{ "purple" };
static constexpr std::string_view CyanKey{ "cyan" };
static constexpr std::string_view WhiteKey{ "white" };
static constexpr std::string_view BrightBlackKey{ "brightBlack" };
static constexpr std::string_view BrightRedKey{ "brightRed" };
static constexpr std::string_view BrightGreenKey{ "brightGreen" };
static constexpr std::string_view BrightYellowKey{ "brightYellow" };
static constexpr std::string_view BrightBlueKey{ "brightBlue" };
static constexpr std::string_view BrightPurpleKey{ "brightPurple" };
static constexpr std::string_view BrightCyanKey{ "brightCyan" };
static constexpr std::string_view BrightWhiteKey{ "brightWhite" };

static constexpr std::array<std::string_view, 16> TableColors = {
    BlackKey,
    RedKey,
    GreenKey,
    YellowKey,
    BlueKey,
    PurpleKey,
    CyanKey,
    WhiteKey,
    BrightBlackKey,
    BrightRedKey,
    BrightGreenKey,
    BrightYellowKey,
    BrightBlueKey,
    BrightPurpleKey,
    BrightCyanKey,
    BrightWhiteKey,
};

ColorScheme::ColorScheme(winrt::hstring name, Color defaultFg, Color defaultBg, Color cursorColor) :
    _Name{ name },
    _Foreground{ defaultFg },
    _Background{ defaultBg },
    _CursorColor{ cursorColor }
{
}

// Method Description:
// - Apply our values to the given TerminalSettings object. Sets the foreground,
//      background, and color table of the settings object.
// Arguments:
// - terminalSettings: the object to apply our settings to.
// Return Value:
// - <none>
void ColorScheme::ApplyScheme(const winrt::Microsoft::Terminal::TerminalControl::IControlSettings& terminalSettings) const
{
    terminalSettings.DefaultForeground(static_cast<COLORREF>(_Foreground));
    terminalSettings.DefaultBackground(static_cast<COLORREF>(_Background));
    terminalSettings.SelectionBackground(static_cast<COLORREF>(_SelectionBackground));
    terminalSettings.CursorColor(static_cast<COLORREF>(_CursorColor));

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

    int i = 0;
    for (const auto& current : TableColors)
    {
        JsonUtils::GetValueForKey(json, current, _table.at(i));
        i++;
    }
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - scheme: a ColorScheme object that will be converted into a serialized JsonObject
// Return Value:
// - a serialized JsonObject from the values in scheme
Json::Value ColorScheme::ToJson(const TerminalApp::ColorScheme& scheme)
{
    Json::Value json{ Json::ValueType::objectValue };
    const auto schemeImpl{ winrt::get_self<implementation::ColorScheme>(scheme) };
    schemeImpl->UpdateJson(json);
    return json;
}

// Method Description:
// - Update the given json object with values from this object.
// Arguments:
// - json: an object which will be a serialization of a ColorScheme object.
// Return Value:
// <none>
void ColorScheme::UpdateJson(Json::Value& json)
{
    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);

    int i = 0;
    for (const auto& current : TableColors)
    {
        JsonUtils::SetValueForKey(json, current, _table.at(i));
        i++;
    }
}

winrt::com_array<Color> ColorScheme::Table() const noexcept
{
    winrt::com_array<Color> result{ COLOR_TABLE_SIZE };
    std::transform(_table.begin(), _table.end(), result.begin(), [](til::color c) -> Color { return c; });
    return result;
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
