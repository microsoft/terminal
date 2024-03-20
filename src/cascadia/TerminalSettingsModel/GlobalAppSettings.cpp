// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "JsonUtils.h"
#include "KeyChordSerialization.h"

#include "GlobalAppSettings.g.cpp"

#include <LibraryResources.h>

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
static constexpr std::string_view LegacyReloadEnvironmentVariablesKey{ "compatibility.reloadEnvironmentVariables" };

// Method Description:
// - Copies any extraneous data from the parent before completing a CreateChild call
// Arguments:
// - <none>
// Return Value:
// - <none>
void GlobalAppSettings::_FinalizeInheritance()
{
    for (const auto& parent : _parents)
    {
        _actionMap->AddLeastImportantParent(parent->_actionMap);
        _keybindingsWarnings.insert(_keybindingsWarnings.end(), parent->_keybindingsWarnings.begin(), parent->_keybindingsWarnings.end());

        for (const auto& [k, v] : parent->_themes)
        {
            if (!_themes.HasKey(k))
            {
                _themes.Insert(k, v);
            }
        }
    }
}

winrt::com_ptr<GlobalAppSettings> GlobalAppSettings::Copy() const
{
    auto globals{ winrt::make_self<GlobalAppSettings>() };

    globals->_UnparsedDefaultProfile = _UnparsedDefaultProfile;

    globals->_defaultProfile = _defaultProfile;
    globals->_actionMap = _actionMap->Copy();
    globals->_keybindingsWarnings = _keybindingsWarnings;

#define GLOBAL_SETTINGS_COPY(type, name, jsonKey, ...) \
    globals->_##name = _##name;
    MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_COPY)
#undef GLOBAL_SETTINGS_COPY

    if (_colorSchemes)
    {
        for (auto kv : _colorSchemes)
        {
            const auto schemeImpl{ winrt::get_self<ColorScheme>(kv.Value()) };
            globals->_colorSchemes.Insert(kv.Key(), *schemeImpl->Copy());
        }
    }
    if (_themes)
    {
        for (auto kv : _themes)
        {
            const auto themeImpl{ winrt::get_self<implementation::Theme>(kv.Value()) };
            globals->_themes.Insert(kv.Key(), *themeImpl->Copy());
        }
    }

    for (const auto& parent : _parents)
    {
        globals->AddLeastImportantParent(parent->Copy());
    }
    return globals;
}

winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::ColorScheme> GlobalAppSettings::ColorSchemes() noexcept
{
    return _colorSchemes.GetView();
}

#pragma region DefaultProfile

void GlobalAppSettings::DefaultProfile(const winrt::guid& defaultProfile) noexcept
{
    _defaultProfile = defaultProfile;
    _UnparsedDefaultProfile = Utils::GuidToString(defaultProfile);
}

winrt::guid GlobalAppSettings::DefaultProfile() const
{
    return _defaultProfile;
}

#pragma endregion

winrt::Microsoft::Terminal::Settings::Model::ActionMap GlobalAppSettings::ActionMap() const noexcept
{
    return *_actionMap;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a GlobalAppSettings object.
// Return Value:
// - a new GlobalAppSettings instance created from the values in `json`
winrt::com_ptr<GlobalAppSettings> GlobalAppSettings::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<GlobalAppSettings>();
    result->LayerJson(json);
    return result;
}

void GlobalAppSettings::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);
    // GH#8076 - when adding enum values to this key, we also changed it from
    // "useTabSwitcher" to "tabSwitcherMode". Continue supporting
    // "useTabSwitcher", but prefer "tabSwitcherMode"
    JsonUtils::GetValueForKey(json, LegacyUseTabSwitcherModeKey, _TabSwitcherMode);

#define GLOBAL_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);
    MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_LAYER_JSON)
#undef GLOBAL_SETTINGS_LAYER_JSON

    LayerActionsFrom(json, true);

    JsonUtils::GetValueForKey(json, LegacyReloadEnvironmentVariablesKey, _legacyReloadEnvironmentVariables);
}

void GlobalAppSettings::LayerActionsFrom(const Json::Value& json, const bool withKeybindings)
{
    static constexpr std::array bindingsKeys{ LegacyKeybindingsKey, ActionsKey };
    for (const auto& jsonKey : bindingsKeys)
    {
        if (auto bindings{ json[JsonKey(jsonKey)] })
        {
            auto warnings = _actionMap->LayerJson(bindings, withKeybindings);

            // It's possible that the user provided keybindings have some warnings
            // in them - problems that we should alert the user to, but we can
            // recover from. Most of these warnings cannot be detected later in the
            // Validate settings phase, so we'll collect them now. If there were any
            // warnings generated from parsing these keybindings, add them to our
            // list of warnings.
            _keybindingsWarnings.insert(_keybindingsWarnings.end(), warnings.begin(), warnings.end());
        }
    }
}

// Method Description:
// - Adds the given colorscheme to our map of schemes, using its name as the key.
// Arguments:
// - scheme: the color scheme to add
// Return Value:
// - <none>
void GlobalAppSettings::AddColorScheme(const Model::ColorScheme& scheme)
{
    _colorSchemes.Insert(scheme.Name(), scheme);
}

void GlobalAppSettings::RemoveColorScheme(hstring schemeName)
{
    _colorSchemes.TryRemove(schemeName);
}

winrt::Microsoft::Terminal::Settings::Model::ColorScheme GlobalAppSettings::DuplicateColorScheme(const Model::ColorScheme& source)
{
    THROW_HR_IF_NULL(E_INVALIDARG, source);

    auto newName = fmt::format(FMT_COMPILE(L"{} ({})"), source.Name(), RS_(L"CopySuffix"));
    auto nextCandidateIndex = 2;

    // Check if this name already exists and if so, append a number
    while (_colorSchemes.HasKey(newName))
    {
        // There is a theoretical unsigned integer wraparound, which is OK
        newName = fmt::format(FMT_COMPILE(L"{} ({} {})"), source.Name(), RS_(L"CopySuffix"), nextCandidateIndex++);
    }

    auto duplicated{ winrt::get_self<ColorScheme>(source)->Copy() };
    duplicated->Name(hstring{ newName });
    duplicated->Origin(OriginTag::User);

    AddColorScheme(*duplicated);
    return *duplicated;
}

// Method Description:
// - Return the warnings that we've collected during parsing the JSON for the
//   keybindings. It's possible that the user provided keybindings have some
//   warnings in them - problems that we should alert the user to, but we can
//   recover from.
// Arguments:
// - <none>
// Return Value:
// - <none>
const std::vector<winrt::Microsoft::Terminal::Settings::Model::SettingsLoadWarnings>& GlobalAppSettings::KeybindingsWarnings() const
{
    return _keybindingsWarnings;
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value GlobalAppSettings::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);

#define GLOBAL_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_TO_JSON)
#undef GLOBAL_SETTINGS_TO_JSON

    json[JsonKey(ActionsKey)] = _actionMap->ToJson();
    return json;
}

winrt::Microsoft::Terminal::Settings::Model::Theme GlobalAppSettings::CurrentTheme() noexcept
{
    auto requestedTheme = Model::Theme::IsSystemInDarkTheme() ?
                              winrt::Windows::UI::Xaml::ElementTheme::Dark :
                              winrt::Windows::UI::Xaml::ElementTheme::Light;

    switch (requestedTheme)
    {
    case winrt::Windows::UI::Xaml::ElementTheme::Light:
        return _themes.TryLookup(Theme().LightName());

    case winrt::Windows::UI::Xaml::ElementTheme::Dark:
        return _themes.TryLookup(Theme().DarkName());

    case winrt::Windows::UI::Xaml::ElementTheme::Default:
    default:
        return nullptr;
    }
}

void GlobalAppSettings::AddTheme(const Model::Theme& theme)
{
    _themes.Insert(theme.Name(), theme);
}

winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::Theme> GlobalAppSettings::Themes() noexcept
{
    return _themes.GetView();
}

void GlobalAppSettings::ExpandCommands(const winrt::Windows::Foundation::Collections::IVectorView<Model::Profile>& profiles,
                                       const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, Model::ColorScheme>& schemes)
{
    _actionMap->ExpandCommands(profiles, schemes);
}

bool GlobalAppSettings::ShouldUsePersistedLayout() const
{
    return FirstWindowPreference() == FirstWindowPreference::PersistedWindowLayout && !IsolatedMode();
}
