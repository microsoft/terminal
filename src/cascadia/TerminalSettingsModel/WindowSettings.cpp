// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowSettings.h"
#include "../../types/inc/Utils.hpp"
#include "JsonUtils.h"
#include "KeyChordSerialization.h"

#include "WindowSettings.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::UI::Xaml::Controls;

static constexpr std::string_view LegacyKeybindingsKey{ "keybindings" };
static constexpr std::string_view ActionsKey{ "actions" };
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

// winrt::Microsoft::Terminal::Settings::Model::Theme WindowSettings::CurrentTheme() noexcept
// {
//     auto requestedTheme = Model::Theme::IsSystemInDarkTheme() ?
//                               winrt::Windows::UI::Xaml::ElementTheme::Dark :
//                               winrt::Windows::UI::Xaml::ElementTheme::Light;

//     switch (requestedTheme)
//     {
//     case winrt::Windows::UI::Xaml::ElementTheme::Light:
//         return _themes.TryLookup(Theme().LightName());

//     case winrt::Windows::UI::Xaml::ElementTheme::Dark:
//         return _themes.TryLookup(Theme().DarkName());

//     case winrt::Windows::UI::Xaml::ElementTheme::Default:
//     default:
//         return nullptr;
//     }
// }
