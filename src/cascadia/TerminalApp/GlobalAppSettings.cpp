// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "JsonUtils.h"

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
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

static constexpr std::string_view DebugFeaturesKey{ "debugFeatures" };

static constexpr std::string_view ForceFullRepaintRenderingKey{ "experimental.rendering.forceFullRepaint" };
static constexpr std::string_view SoftwareRenderingKey{ "experimental.rendering.software" };
static constexpr std::string_view ForceVTInputKey{ "experimental.input.forceVT" };

// Launch mode values
static constexpr std::string_view DefaultLaunchModeValue{ "default" };
static constexpr std::string_view MaximizedLaunchModeValue{ "maximized" };
static constexpr std::string_view FullscreenLaunchModeValue{ "fullscreen" };

// Tab Width Mode values
static constexpr std::string_view EqualTabWidthModeValue{ "equal" };
static constexpr std::string_view TitleLengthTabWidthModeValue{ "titleLength" };
static constexpr std::string_view TitleLengthCompactModeValue{ "compact" };

// Theme values
static constexpr std::string_view LightThemeValue{ "light" };
static constexpr std::string_view DarkThemeValue{ "dark" };
static constexpr std::string_view SystemThemeValue{ "system" };

#ifdef _DEBUG
static constexpr bool debugFeaturesDefault{ true };
#else
static constexpr bool debugFeaturesDefault{ false };
#endif

template<>
struct JsonUtils::ConversionTrait<ElementTheme> : public JsonUtils::KeyValueMapper<ElementTheme, JsonUtils::ConversionTrait<ElementTheme>>
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ SystemThemeValue, ElementTheme::Default },
        pair_type{ LightThemeValue, ElementTheme::Light },
        pair_type{ DarkThemeValue, ElementTheme::Dark },
    };
};

template<>
struct JsonUtils::ConversionTrait<LaunchMode> : public JsonUtils::KeyValueMapper<LaunchMode, JsonUtils::ConversionTrait<LaunchMode>>
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ DefaultLaunchModeValue, LaunchMode::DefaultMode },
        pair_type{ MaximizedLaunchModeValue, LaunchMode::MaximizedMode },
        pair_type{ FullscreenLaunchModeValue, LaunchMode::FullscreenMode },
    };
};

template<>
struct JsonUtils::ConversionTrait<TabViewWidthMode> : public JsonUtils::KeyValueMapper<TabViewWidthMode, JsonUtils::ConversionTrait<TabViewWidthMode>>
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ EqualTabWidthModeValue, TabViewWidthMode::Equal },
        pair_type{ TitleLengthTabWidthModeValue, TabViewWidthMode::SizeToContent },
        pair_type{ TitleLengthCompactModeValue, TabViewWidthMode::Compact },
    };
};

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
            _keybindingsWarnings.insert(_keybindingsWarnings.end(), warnings.begin(), warnings.end());
        }
    };
    parseBindings(LegacyKeybindingsKey);
    parseBindings(BindingsKey);
}

// Method Description:
// - Helper function for converting the initial position string into
//   2 coordinate values. We allow users to only provide one coordinate,
//   thus, we use comma as the separator:
//   (100, 100): standard input string
//   (, 100), (100, ): if a value is missing, we set this value as a default
//   (,): both x and y are set to default
//   (abc, 100): if a value is not valid, we treat it as default
//   (100, 100, 100): we only read the first two values, this is equivalent to (100, 100)
// Arguments:
// - initialPosition: the initial position string from json
//   initialX: reference to the _initialX member
//   initialY: reference to the _initialY member
// Return Value:
// - None
template<>
struct JsonUtils::ConversionTrait<LaunchPosition>
{
    LaunchPosition FromJson(const Json::Value& json)
    {
        LaunchPosition ret;
        std::string initialPosition{ json.asString() };
        static constexpr char singleCharDelim = ',';
        std::stringstream tokenStream(initialPosition);
        std::string token;
        uint8_t initialPosIndex = 0;

        // Get initial position values till we run out of delimiter separated values in the stream
        // or we hit max number of allowable values (= 2)
        // Non-numeral values or empty string will be caught as exception and we do not assign them
        for (; std::getline(tokenStream, token, singleCharDelim) && (initialPosIndex < 2); initialPosIndex++)
        {
            try
            {
                int32_t position = std::stoi(token);
                if (initialPosIndex == 0)
                {
                    ret.x.emplace(position);
                }

                if (initialPosIndex == 1)
                {
                    ret.y.emplace(position);
                }
            }
            catch (...)
            {
                // Do nothing
            }
        }
        return ret;
    }

    bool CanConvert(const Json::Value& json)
    {
        return json.isString();
    }
};

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
