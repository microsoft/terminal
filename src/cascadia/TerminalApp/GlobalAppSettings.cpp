// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../inc/DefaultSettings.h"

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;

static const std::wstring DEFAULTPROFILE_KEY{ L"defaultProfile" };
static const std::wstring ALWAYS_SHOW_TABS_KEY{ L"alwaysShowTabs" };
static const std::wstring INITIALROWS_KEY{ L"initialRows" };
static const std::wstring INITIALCOLS_KEY{ L"initialCols" };
static const std::wstring SHOW_TITLE_IN_TITLEBAR_KEY{ L"showTerminalTitleInTitlebar" };
static const std::wstring REQUESTED_THEME_KEY{ L"requestedTheme" };

static const std::wstring SHOW_TABS_IN_TITLEBAR_KEY{ L"experimental_showTabsInTitlebar" };

static const std::wstring LIGHT_THEME_VALUE{ L"light" };
static const std::wstring DARK_THEME_VALUE{ L"dark" };
static const std::wstring SYSTEM_THEME_VALUE{ L"system" };

GlobalAppSettings::GlobalAppSettings() :
    _keybindings{},
    _colorSchemes{},
    _defaultProfile{},
    _alwaysShowTabs{ false },
    _initialRows{ DEFAULT_ROWS },
    _initialCols{ DEFAULT_COLS },
    _showTitleInTitlebar{ true },
    _showTabsInTitlebar{ false },
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
JsonObject GlobalAppSettings::ToJson() const
{
    winrt::Windows::Data::Json::JsonObject jsonObject;

    const auto guidStr = Utils::GuidToString(_defaultProfile);
    const auto defaultProfile = JsonValue::CreateStringValue(guidStr);
    const auto initialRows = JsonValue::CreateNumberValue(_initialRows);
    const auto initialCols = JsonValue::CreateNumberValue(_initialCols);

    jsonObject.Insert(DEFAULTPROFILE_KEY, defaultProfile);
    jsonObject.Insert(INITIALROWS_KEY, initialRows);
    jsonObject.Insert(INITIALCOLS_KEY, initialCols);
    jsonObject.Insert(ALWAYS_SHOW_TABS_KEY,
                      JsonValue::CreateBooleanValue(_alwaysShowTabs));
    jsonObject.Insert(SHOW_TITLE_IN_TITLEBAR_KEY,
                      JsonValue::CreateBooleanValue(_showTitleInTitlebar));

    jsonObject.Insert(SHOW_TABS_IN_TITLEBAR_KEY,
                      JsonValue::CreateBooleanValue(_showTabsInTitlebar));
    if (_requestedTheme != ElementTheme::Default)
    {
        jsonObject.Insert(REQUESTED_THEME_KEY,
                          JsonValue::CreateStringValue(_SerializeTheme(_requestedTheme)));
    }

    return jsonObject;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a GlobalAppSettings object.
// Return Value:
// - a new GlobalAppSettings instance created from the values in `json`
GlobalAppSettings GlobalAppSettings::FromJson(winrt::Windows::Data::Json::JsonObject json)
{
    GlobalAppSettings result{};

    if (json.HasKey(DEFAULTPROFILE_KEY))
    {
        auto guidString = json.GetNamedString(DEFAULTPROFILE_KEY);
        auto guid = Utils::GuidFromString(guidString.c_str());
        result._defaultProfile = guid;
    }

    if (json.HasKey(ALWAYS_SHOW_TABS_KEY))
    {
        result._alwaysShowTabs = json.GetNamedBoolean(ALWAYS_SHOW_TABS_KEY);
    }
    if (json.HasKey(INITIALROWS_KEY))
    {
        result._initialRows = static_cast<int32_t>(json.GetNamedNumber(INITIALROWS_KEY));
    }
    if (json.HasKey(INITIALCOLS_KEY))
    {
        result._initialCols = static_cast<int32_t>(json.GetNamedNumber(INITIALCOLS_KEY));
    }

    if (json.HasKey(SHOW_TITLE_IN_TITLEBAR_KEY))
    {
        result._showTitleInTitlebar = json.GetNamedBoolean(SHOW_TITLE_IN_TITLEBAR_KEY);
    }

    if (json.HasKey(SHOW_TABS_IN_TITLEBAR_KEY))
    {
        result._showTabsInTitlebar = json.GetNamedBoolean(SHOW_TABS_IN_TITLEBAR_KEY);
    }

    if (json.HasKey(REQUESTED_THEME_KEY))
    {
        const auto themeStr = json.GetNamedString(REQUESTED_THEME_KEY);
        result._requestedTheme = _ParseTheme(themeStr.c_str());
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
    if (themeString == LIGHT_THEME_VALUE)
    {
        return ElementTheme::Light;
    }
    else if (themeString == DARK_THEME_VALUE)
    {
        return ElementTheme::Dark;
    }
    // default behavior for invalid data or SYSTEM_THEME_VALUE
    return ElementTheme::Default;
}

// Method Description:
// - Helper function for converting a CursorStyle to it's corresponding string
//   value.
// Arguments:
// - theme: The enum value to convert to a string.
// Return Value:
// - The string value for the given CursorStyle
std::wstring GlobalAppSettings::_SerializeTheme(const ElementTheme theme) noexcept
{
    switch (theme)
    {
        case ElementTheme::Light:
            return LIGHT_THEME_VALUE;
        case ElementTheme::Dark:
            return DARK_THEME_VALUE;
        default:
            return SYSTEM_THEME_VALUE;
    }
}
