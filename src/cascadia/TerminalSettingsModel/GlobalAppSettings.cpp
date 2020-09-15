// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "../TerminalApp/Utils.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "GlobalAppSettings.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::UI::Xaml::Controls;

static constexpr std::string_view LegacyKeybindingsKey{ "keybindings" };
static constexpr std::string_view ActionsKey{ "actions" };
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
static constexpr std::string_view UseTabSwitcherKey{ "useTabSwitcher" };

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
    _keymap{ winrt::make_self<KeyMapping>() },
    _keybindingsWarnings{},
    _unparsedDefaultProfile{},
    _defaultProfile{},
    _DebugFeaturesEnabled{ debugFeaturesDefault }
{
    _commands = winrt::single_threaded_map<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::Command>();
    _colorSchemes = winrt::single_threaded_map<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::ColorScheme>();
}

winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::ColorScheme> GlobalAppSettings::ColorSchemes() noexcept
{
    return _colorSchemes.GetView();
}

void GlobalAppSettings::DefaultProfile(const winrt::guid& defaultProfile) noexcept
{
    _unparsedDefaultProfile.clear();
    _defaultProfile = defaultProfile;
}

winrt::guid GlobalAppSettings::DefaultProfile() const
{
    // If we have an unresolved default profile, we should likely explode.
    THROW_HR_IF(E_INVALIDARG, !_unparsedDefaultProfile.empty());
    return _defaultProfile;
}

winrt::hstring GlobalAppSettings::UnparsedDefaultProfile() const
{
    return _unparsedDefaultProfile;
}

winrt::Microsoft::Terminal::Settings::Model::KeyMapping GlobalAppSettings::KeyMap() const noexcept
{
    return *_keymap;
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

    JsonUtils::GetValueForKey(json, UseTabSwitcherKey, _UseTabSwitcher);

    // This is a helper lambda to get the keybindings and commands out of both
    // and array of objects. We'll use this twice, once on the legacy
    // `keybindings` key, and again on the newer `bindings` key.
    auto parseBindings = [this, &json](auto jsonKey) {
        if (auto bindings{ json[JsonKey(jsonKey)] })
        {
            auto warnings = _keymap->LayerJson(bindings);
            // It's possible that the user provided keybindings have some warnings
            // in them - problems that we should alert the user to, but we can
            // recover from. Most of these warnings cannot be detected later in the
            // Validate settings phase, so we'll collect them now. If there were any
            // warnings generated from parsing these keybindings, add them to our
            // list of warnings.
            _keybindingsWarnings.insert(_keybindingsWarnings.end(), warnings.begin(), warnings.end());

            // Now parse the array again, but this time as a list of commands.
            warnings = winrt::Microsoft::Terminal::Settings::Model::implementation::Command::LayerJson(_commands, bindings);
        }
    };
    parseBindings(LegacyKeybindingsKey);
    parseBindings(ActionsKey);
}

// Method Description:
// - Adds the given colorscheme to our map of schemes, using its name as the key.
// Arguments:
// - scheme: the color scheme to add
// Return Value:
// - <none>
void GlobalAppSettings::AddColorScheme(const winrt::Microsoft::Terminal::Settings::Model::ColorScheme& scheme)
{
    _colorSchemes.Insert(scheme.Name(), scheme);
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
std::vector<winrt::Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> GlobalAppSettings::KeybindingsWarnings() const
{
    return _keybindingsWarnings;
}

winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::Command> GlobalAppSettings::Commands() noexcept
{
    return _commands.GetView();
}
