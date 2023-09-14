;
;
; // Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowSettings.h"
#include "../../types/inc/Utils.hpp"
#include "JsonUtils.h"
#include "KeyChordSerialization.h"
#include "../inc/WindowingBehavior.h"

#include "WindowSettings.g.cpp"
#include "Docking.g.cpp"

#include "ProfileEntry.h"
#include "FolderEntry.h"
#include "MatchProfilesEntry.h"

using namespace ::Microsoft::Console;
using namespace ::Microsoft::Terminal::Settings::Model;

using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Microsoft::Terminal::Settings;

using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::Foundation::Collections;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view ThemeKey{ "theme" };
static constexpr std::string_view DefaultProfileKey{ "defaultProfile" };
static constexpr std::string_view LegacyUseTabSwitcherModeKey{ "useTabSwitcher" };

// Method Description:
// - Copies any extraneous data from the parent before completing a CreateChild call
// Arguments:
// - <none>
// Return Value:
// - <none>
void WindowSettings::_FinalizeInheritance()
{
    for (const auto& parent : _parents)
    {
        parent;

        // _actionMap->AddLeastImportantParent(parent->_actionMap);
        // _keybindingsWarnings.insert(_keybindingsWarnings.end(), parent->_keybindingsWarnings.begin(), parent->_keybindingsWarnings.end());
        // for (const auto& [k, v] : parent->_colorSchemes)
        // {
        //     if (!_colorSchemes.HasKey(k))
        //     {
        //         _colorSchemes.Insert(k, v);
        //     }
        // }

        // for (const auto& [k, v] : parent->_themes)
        // {
        //     if (!_themes.HasKey(k))
        //     {
        //         _themes.Insert(k, v);
        //     }
        // }
    }
}

winrt::com_ptr<WindowSettings> WindowSettings::Copy() const
{
    auto globals{ winrt::make_self<WindowSettings>() };

    globals->_UnparsedDefaultProfile = _UnparsedDefaultProfile;
    globals->_defaultProfile = _defaultProfile;

#define WINDOW_SETTINGS_COPY(type, name, jsonKey, ...) \
    globals->_##name = _##name;
    MTSM_WINDOW_SETTINGS(WINDOW_SETTINGS_COPY)
#undef WINDOW_SETTINGS_COPY

    for (const auto& parent : _parents)
    {
        globals->AddLeastImportantParent(parent->Copy());
    }
    return globals;
}

#pragma region DefaultProfile

void WindowSettings::DefaultProfile(const winrt::guid& defaultProfile) noexcept
{
    _defaultProfile = defaultProfile;
    _UnparsedDefaultProfile = Utils::GuidToString(defaultProfile);
}

winrt::guid WindowSettings::DefaultProfile() const
{
    return _defaultProfile;
}

#pragma endregion

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a WindowSettings object.
// Return Value:
// - a new WindowSettings instance created from the values in `json`
winrt::com_ptr<WindowSettings> WindowSettings::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<WindowSettings>();
    result->LayerJson(json);
    return result;
}

void WindowSettings::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, NameKey, Name);

    // sneaky: is the name _quake? then set up some default window settings here:
    if (Name() == QuakeWindowName)
    {
        InitializeForQuakeMode();
    }

    JsonUtils::GetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);
    // GH#8076 - when adding enum values to this key, we also changed it from
    // "useTabSwitcher" to "tabSwitcherMode". Continue supporting
    // "useTabSwitcher", but prefer "tabSwitcherMode"
    JsonUtils::GetValueForKey(json, LegacyUseTabSwitcherModeKey, _TabSwitcherMode);

#define WINDOW_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);
    MTSM_WINDOW_SETTINGS(WINDOW_SETTINGS_LAYER_JSON)
#undef WINDOW_SETTINGS_LAYER_JSON
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value WindowSettings::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);

#define WINDOW_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_WINDOW_SETTINGS(WINDOW_SETTINGS_TO_JSON)
#undef WINDOW_SETTINGS_TO_JSON

    return json;
}

// Set up anything that we need that's quake-mode specific.
void WindowSettings::InitializeForQuakeMode()
{
    LaunchMode(LaunchMode::FocusMode);

    auto dockSettings{ winrt::make_self<Docking>() };
    dockSettings->Side(Model::DockPosition::Top);
    dockSettings->Width(1.0);
    dockSettings->Height(0.5);
    _DockWindow = *dockSettings;
    _MinimizeToNotificationArea = true;
}

static constexpr std::string_view SideKey{ "side" };
static constexpr std::string_view WidthKey{ "width" };
static constexpr std::string_view HeightKey{ "height" };

winrt::com_ptr<Docking> Docking::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<Docking>();

    if (json.isObject())
    {
        JsonUtils::GetValueForKey(json, SideKey, result->Side);
        JsonUtils::GetValueForKey(json, WidthKey, result->Width);
        JsonUtils::GetValueForKey(json, HeightKey, result->Height);
    }
    return result;
}

Json::Value Docking::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, SideKey, Side);
    JsonUtils::SetValueForKey(json, WidthKey, Width);
    JsonUtils::SetValueForKey(json, HeightKey, Height);
    return json;
}
winrt::com_ptr<Docking> Docking::Copy() const
{
    auto pair{ winrt::make_self<Docking>() };
    pair->Side = Side;
    pair->Width = Width;
    pair->Height = Height;
    return pair;
}
