// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::UI::Xaml::Controls;

static constexpr std::string_view LegacyKeybindingsKey{ "keybindings" };
static constexpr std::string_view BindingsKey{ "bindings" };
static constexpr std::string_view DefaultProfileKey{ "defaultProfile" };
static constexpr std::string_view AlwaysShowTabsKey{ "alwaysShowTabs" };
static constexpr std::string_view InitialRowsKey{ "initialRows" };
static constexpr std::string_view InitialColsKey{ "initialCols" };
static constexpr std::string_view InitialPositionKey{ "initialPosition" };
static constexpr std::string_view ShowTitleInTitlebarKey{ "showTerminalTitleInTitlebar" };
static constexpr std::string_view ThemeKey{ "theme" };
static constexpr std::string_view TabWidthModeKey{ "tabWidthMode" };
static constexpr std::string_view ShowTabsInTitlebarKey{ "showTabsInTitlebar" };
static constexpr std::string_view WordDelimitersKey{ "wordDelimiters" };
static constexpr std::string_view CopyOnSelectKey{ "copyOnSelect" };
static constexpr std::string_view CopyFormattingKey{ "copyFormatting" };
static constexpr std::string_view WarnAboutLargePasteKey{ "largePasteWarning" };
static constexpr std::string_view WarnAboutMultiLinePasteKey{ "multiLinePasteWarning" };
static constexpr std::string_view LaunchModeKey{ "launchMode" };
static constexpr std::string_view ConfirmCloseAllKey{ "confirmCloseAllTabs" };
static constexpr std::string_view SnapToGridOnResizeKey{ "snapToGridOnResize" };
static constexpr std::string_view EnableStartupTaskKey{ "startOnUserLogin" };
static constexpr std::string_view AlwaysOnTopKey{ "alwaysOnTop" };

static constexpr std::string_view DebugFeaturesKey{ "debugFeatures" };

static constexpr std::string_view ForceFullRepaintRenderingKey{ "experimental.rendering.forceFullRepaint" };
static constexpr std::string_view SoftwareRenderingKey{ "experimental.rendering.software" };
static constexpr std::string_view ForceVTInputKey{ "experimental.input.forceVT" };

#ifdef _DEBUG
static constexpr bool debugFeaturesDefault{ true };
#else
static constexpr bool debugFeaturesDefault{ false };
#endif

GlobalAppSettings::GlobalAppSettings() :
    _keybindings{ winrt::make_self<winrt::TerminalApp::implementation::AppKeyBindings>() },
    _keybindingsWarnings{},
    _colorSchemes{},
    _unparsedDefaultProfile{ std::nullopt },
    _defaultProfile{},
    _InitialRows{ DEFAULT_ROWS },
    _InitialCols{ DEFAULT_COLS },
    _WordDelimiters{ DEFAULT_WORD_DELIMITERS },
    _DebugFeaturesEnabled{ debugFeaturesDefault }
{
}

GlobalAppSettings::~GlobalAppSettings()
{
}

std::unordered_map<std::wstring, ColorScheme>& GlobalAppSettings::GetColorSchemes() noexcept
{
    return _colorSchemes;
}

const std::unordered_map<std::wstring, ColorScheme>& GlobalAppSettings::GetColorSchemes() const noexcept
{
    return _colorSchemes;
}

void GlobalAppSettings::DefaultProfile(const GUID defaultProfile) noexcept
{
    _unparsedDefaultProfile.reset();
    _defaultProfile = defaultProfile;
}

GUID GlobalAppSettings::DefaultProfile() const
{
    // If we have an unresolved default profile, we should likely explode.
    THROW_HR_IF(E_INVALIDARG, _unparsedDefaultProfile.has_value());
    return _defaultProfile;
}

std::wstring GlobalAppSettings::UnparsedDefaultProfile() const
{
    return _unparsedDefaultProfile.value();
}

AppKeyBindings GlobalAppSettings::GetKeybindings() const noexcept
{
    return *_keybindings;
}

// Method Description:
// - Applies appropriate settings from the globals into the given TerminalSettings.
// Arguments:
// - settings: a TerminalSettings object to add global property values to.
// Return Value:
// - <none>
void GlobalAppSettings::ApplyToSettings(TerminalSettings& settings) const noexcept
{
    settings.KeyBindings(GetKeybindings());
    settings.InitialRows(_InitialRows);
    settings.InitialCols(_InitialCols);

    settings.WordDelimiters(_WordDelimiters);
    settings.CopyOnSelect(_CopyOnSelect);
    settings.ForceFullRepaintRendering(_ForceFullRepaintRendering);
    settings.SoftwareRendering(_SoftwareRendering);
    settings.ForceVTInput(_ForceVTInput);
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a GlobalAppSettings object.
// Return Value:
// - a new GlobalAppSettings instance created from the values in `json`
GlobalAppSettings GlobalAppSettings::FromJson(const Json::Value& json)
{
    GlobalAppSettings result;
    result.LayerJson(json);
    return result;
}

void GlobalAppSettings::LayerJson(const Json::Value& json)
{
    JsonUtils::GetValueForKey(json, DefaultProfileKey, _unparsedDefaultProfile);

    JsonUtils::GetValueForKey(json, AlwaysShowTabsKey, _AlwaysShowTabs);

    JsonUtils::GetValueForKey(json, ConfirmCloseAllKey, _ConfirmCloseAllTabs);

    JsonUtils::GetValueForKey(json, InitialRowsKey, _InitialRows);

    JsonUtils::GetValueForKey(json, InitialColsKey, _InitialCols);

    JsonUtils::GetValueForKey(json, InitialPositionKey, _InitialPosition);

    JsonUtils::GetValueForKey(json, ShowTitleInTitlebarKey, _ShowTitleInTitlebar);

    JsonUtils::GetValueForKey(json, ShowTabsInTitlebarKey, _ShowTabsInTitlebar);

    JsonUtils::GetValueForKey(json, WordDelimitersKey, _WordDelimiters);

    JsonUtils::GetValueForKey(json, CopyOnSelectKey, _CopyOnSelect);

    JsonUtils::GetValueForKey(json, CopyFormattingKey, _CopyFormatting);

    JsonUtils::GetValueForKey(json, WarnAboutLargePasteKey, _WarnAboutLargePaste);

    JsonUtils::GetValueForKey(json, WarnAboutMultiLinePasteKey, _WarnAboutMultiLinePaste);

    JsonUtils::GetValueForKey(json, LaunchModeKey, _LaunchMode);

    JsonUtils::GetValueForKey(json, ThemeKey, _Theme);

    JsonUtils::GetValueForKey(json, TabWidthModeKey, _TabWidthMode);

    JsonUtils::GetValueForKey(json, SnapToGridOnResizeKey, _SnapToGridOnResize);

    // GetValueForKey will only override the current value if the key exists
    JsonUtils::GetValueForKey(json, DebugFeaturesKey, _DebugFeaturesEnabled);

    JsonUtils::GetValueForKey(json, ForceFullRepaintRenderingKey, _ForceFullRepaintRendering);

    JsonUtils::GetValueForKey(json, SoftwareRenderingKey, _SoftwareRendering);
    JsonUtils::GetValueForKey(json, ForceVTInputKey, _ForceVTInput);

    JsonUtils::GetValueForKey(json, EnableStartupTaskKey, _StartOnUserLogin);

    JsonUtils::GetValueForKey(json, AlwaysOnTopKey, _AlwaysOnTop);

    // This is a helper lambda to get the keybindings and commands out of both
    // and array of objects. We'll use this twice, once on the legacy
    // `keybindings` key, and again on the newer `bindings` key.
    auto parseBindings = [this, &json](auto jsonKey) {
        if (auto bindings{ json[JsonKey(jsonKey)] })
        {
            auto warnings = _keybindings->LayerJson(bindings);
            // It's possible that the user provided keybindings have some warnings
            // in them - problems that we should alert the user to, but we can
            // recover from. Most of these warnings cannot be detected later in the
            // Validate settings phase, so we'll collect them now. If there were any
            // warnings generated from parsing these keybindings, add them to our
            // list of warnings.
            _keybindingsWarnings.insert(_keybindingsWarnings.end(), warnings.begin(), warnings.end());

            // Now parse the array again, but this time as a list of commands.
            warnings = winrt::TerminalApp::implementation::Command::LayerJson(_commands, bindings);
            // It's possible that the user provided commands have some warnings
            // in them, similar to the keybindings.
        }
    };
    parseBindings(LegacyKeybindingsKey);
    parseBindings(BindingsKey);
}

// Method Description:
// - Adds the given colorscheme to our map of schemes, using its name as the key.
// Arguments:
// - scheme: the color scheme to add
// Return Value:
// - <none>
void GlobalAppSettings::AddColorScheme(ColorScheme scheme)
{
    std::wstring name{ scheme.GetName() };
    _colorSchemes[name] = std::move(scheme);
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
std::vector<TerminalApp::SettingsLoadWarnings> GlobalAppSettings::GetKeybindingsWarnings() const
{
    return _keybindingsWarnings;
}

const std::unordered_map<winrt::hstring, winrt::TerminalApp::Command>& GlobalAppSettings::GetCommands() const noexcept
{
    return _commands;
}
