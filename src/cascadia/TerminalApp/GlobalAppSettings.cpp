// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "AppKeyBindingsSerialization.h"
#include "Utils.h"

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;

static constexpr std::string_view KeybindingsKey{ "keybindings" };
static constexpr std::string_view DefaultProfileKey{ "defaultProfile" };
static constexpr std::string_view AlwaysShowTabsKey{ "alwaysShowTabs" };
static constexpr std::string_view InitialRowsKey{ "initialRows" };
static constexpr std::string_view InitialColsKey{ "initialCols" };
static constexpr std::string_view ShowTitleInTitlebarKey{ "showTerminalTitleInTitlebar" };
static constexpr std::string_view RequestedThemeKey{ "requestedTheme" };
static constexpr std::string_view ShowTabsInTitlebarKey{ "showTabsInTitlebar" };

static constexpr std::wstring_view LightThemeValue{ L"light" };
static constexpr std::wstring_view DarkThemeValue{ L"dark" };
static constexpr std::wstring_view SystemThemeValue{ L"system" };

GlobalAppSettings::GlobalAppSettings() :
    _keybindings{},
    _colorSchemes{},
    _defaultProfile{},
    _alwaysShowTabs{ true },
    _initialRows{ DEFAULT_ROWS },
    _initialCols{ DEFAULT_COLS },
    _showTitleInTitlebar{ true },
    _showTabsInTitlebar{ true },
    _requestedTheme{ ElementTheme::Default }
{
}

GlobalAppSettings::~GlobalAppSettings()
{
}

const std::vector<ColorScheme>& GlobalAppSettings::GetColorSchemes() const noexcept
{
    return _colorSchemes;
}

std::vector<ColorScheme>& GlobalAppSettings::GetColorSchemes() noexcept
{
    return _colorSchemes;
}

void GlobalAppSettings::SetDefaultProfile(const GUID defaultProfile) noexcept
{
    _defaultProfile = defaultProfile;
}

GUID GlobalAppSettings::GetDefaultProfile() const noexcept
{
    return _defaultProfile;
}

AppKeyBindings GlobalAppSettings::GetKeybindings() const noexcept
{
    return _keybindings;
}

void GlobalAppSettings::SetKeybindings(winrt::TerminalApp::AppKeyBindings newBindings) noexcept
{
    _keybindings = newBindings;
}

bool GlobalAppSettings::GetAlwaysShowTabs() const noexcept
{
    return _alwaysShowTabs;
}

void GlobalAppSettings::SetAlwaysShowTabs(const bool showTabs) noexcept
{
    _alwaysShowTabs = showTabs;
}

bool GlobalAppSettings::GetShowTitleInTitlebar() const noexcept
{
    return _showTitleInTitlebar;
}

void GlobalAppSettings::SetShowTitleInTitlebar(const bool showTitleInTitlebar) noexcept
{
    _showTitleInTitlebar = showTitleInTitlebar;
}

ElementTheme GlobalAppSettings::GetRequestedTheme() const noexcept
{
    return _requestedTheme;
}

void GlobalAppSettings::SetRequestedTheme(const ElementTheme requestedTheme) noexcept
{
    _requestedTheme = requestedTheme;
}

#pragma region ExperimentalSettings
bool GlobalAppSettings::GetShowTabsInTitlebar() const noexcept
{
    return _showTabsInTitlebar;
}

void GlobalAppSettings::SetShowTabsInTitlebar(const bool showTabsInTitlebar) noexcept
{
    _showTabsInTitlebar = showTabsInTitlebar;
}
#pragma endregion

// Method Description:
// - Applies appropriate settings from the globals into the given TerminalSettings.
// Arguments:
// - settings: a TerminalSettings object to add global property values to.
// Return Value:
// - <none>
void GlobalAppSettings::ApplyToSettings(TerminalSettings& settings) const noexcept
{
    settings.KeyBindings(GetKeybindings());
    settings.InitialRows(_initialRows);
    settings.InitialCols(_initialCols);
}

// Method Description:
// - Serialize this object to a JsonObject.
// Arguments:
// - <none>
// Return Value:
// - a JsonObject which is an equivalent serialization of this object.
Json::Value GlobalAppSettings::ToJson() const
{
    Json::Value jsonObject;

    jsonObject[JsonKey(DefaultProfileKey)] = winrt::to_string(Utils::GuidToString(_defaultProfile));
    jsonObject[JsonKey(InitialRowsKey)] = _initialRows;
    jsonObject[JsonKey(InitialColsKey)] = _initialCols;
    jsonObject[JsonKey(AlwaysShowTabsKey)] = _alwaysShowTabs;
    jsonObject[JsonKey(ShowTitleInTitlebarKey)] = _showTitleInTitlebar;
    jsonObject[JsonKey(ShowTabsInTitlebarKey)] = _showTabsInTitlebar;
    jsonObject[JsonKey(RequestedThemeKey)] = winrt::to_string(_SerializeTheme(_requestedTheme));
    jsonObject[JsonKey(KeybindingsKey)] = AppKeyBindingsSerialization::ToJson(_keybindings);

    return jsonObject;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a GlobalAppSettings object.
// Return Value:
// - a new GlobalAppSettings instance created from the values in `json`
GlobalAppSettings GlobalAppSettings::FromJson(const Json::Value& json)
{
    GlobalAppSettings result{};

    if (auto defaultProfile{ json[JsonKey(DefaultProfileKey)] })
    {
        auto guid = Utils::GuidFromString(GetWstringFromJson(defaultProfile));
        result._defaultProfile = guid;
    }

    if (auto alwaysShowTabs{ json[JsonKey(AlwaysShowTabsKey)] })
    {
        result._alwaysShowTabs = alwaysShowTabs.asBool();
    }
    if (auto initialRows{ json[JsonKey(InitialRowsKey)] })
    {
        result._initialRows = initialRows.asInt();
    }
    if (auto initialCols{ json[JsonKey(InitialColsKey)] })
    {
        result._initialCols = initialCols.asInt();
    }

    if (auto showTitleInTitlebar{ json[JsonKey(ShowTitleInTitlebarKey)] })
    {
        result._showTitleInTitlebar = showTitleInTitlebar.asBool();
    }

    if (auto showTabsInTitlebar{ json[JsonKey(ShowTabsInTitlebarKey)] })
    {
        result._showTabsInTitlebar = showTabsInTitlebar.asBool();
    }

    if (auto requestedTheme{ json[JsonKey(RequestedThemeKey)] })
    {
        result._requestedTheme = _ParseTheme(GetWstringFromJson(requestedTheme));
    }

    if (auto keybindings{ json[JsonKey(KeybindingsKey)] })
    {
        result._keybindings = AppKeyBindingsSerialization::FromJson(keybindings);
    }

    return result;
}

// Method Description:
// - Helper function for converting a user-specified cursor style corresponding
//   CursorStyle enum value
// Arguments:
// - themeString: The string value from the settings file to parse
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
ElementTheme GlobalAppSettings::_ParseTheme(const std::wstring& themeString) noexcept
{
    if (themeString == LightThemeValue)
    {
        return ElementTheme::Light;
    }
    else if (themeString == DarkThemeValue)
    {
        return ElementTheme::Dark;
    }
    // default behavior for invalid data or SystemThemeValue
    return ElementTheme::Default;
}

// Method Description:
// - Helper function for converting a CursorStyle to its corresponding string
//   value.
// Arguments:
// - theme: The enum value to convert to a string.
// Return Value:
// - The string value for the given CursorStyle
std::wstring_view GlobalAppSettings::_SerializeTheme(const ElementTheme theme) noexcept
{
    switch (theme)
    {
    case ElementTheme::Light:
        return LightThemeValue;
    case ElementTheme::Dark:
        return DarkThemeValue;
    default:
        return SystemThemeValue;
    }
}
