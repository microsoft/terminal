// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppSettings.h"
#include "../../types/inc/Utils.hpp"
#include "JsonUtils.h"
#include "KeyChordSerialization.h"

#include "GlobalAppSettings.g.cpp"

#include "MediaResourceSupport.h"

#include <LibraryResources.h>

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;
using namespace winrt::Microsoft::UI::Xaml::Controls;

static constexpr std::string_view KeybindingsKey{ "keybindings" };
static constexpr std::string_view ActionsKey{ "actions" };
static constexpr std::string_view ThemeKey{ "theme" };
static constexpr std::string_view DefaultProfileKey{ "defaultProfile" };
static constexpr std::string_view FirstWindowPreferenceKey{ "firstWindowPreference" };
static constexpr std::string_view LegacyUseTabSwitcherModeKey{ "useTabSwitcher" };
static constexpr std::string_view LegacyReloadEnvironmentVariablesKey{ "compatibility.reloadEnvironmentVariables" };
static constexpr std::string_view LegacyForceVTInputKey{ "experimental.input.forceVT" };
static constexpr std::string_view LegacyInputServiceWarningKey{ "inputServiceWarning" };
static constexpr std::string_view LegacyWarnAboutLargePasteKey{ "largePasteWarning" };
static constexpr std::string_view LegacyWarnAboutMultiLinePasteKey{ "multiLinePasteWarning" };
static constexpr std::string_view LegacyConfirmCloseAllTabsKey{ "confirmCloseAllTabs" };
static constexpr std::string_view LegacyPersistedWindowLayout{ "persistedWindowLayout" };

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
    _actionMap->_FinalizeInheritance();
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
    if (_NewTabMenu)
    {
        globals->_NewTabMenu = winrt::single_threaded_vector<Model::NewTabMenuEntry>();
        for (const auto& entry : *_NewTabMenu)
        {
            globals->_NewTabMenu->Append(get_self<NewTabMenuEntry>(entry)->Copy());
        }
    }
    if (_DisabledProfileSources)
    {
        globals->_DisabledProfileSources = winrt::single_threaded_vector<hstring>();
        for (const auto& src : *_DisabledProfileSources)
        {
            globals->_DisabledProfileSources->Append(src);
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
winrt::com_ptr<GlobalAppSettings> GlobalAppSettings::FromJson(const Json::Value& json, const OriginTag origin)
{
    auto result = winrt::make_self<GlobalAppSettings>();
    result->LayerJson(json, origin);
    return result;
}

void GlobalAppSettings::LayerJson(const Json::Value& json, const OriginTag origin)
{
    JsonUtils::GetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);

    // GH#8076 - when adding enum values to this key, we also changed it from
    // "useTabSwitcher" to "tabSwitcherMode". Continue supporting
    // "useTabSwitcher", but prefer "tabSwitcherMode"
    _fixupsAppliedDuringLoad = JsonUtils::GetValueForKey(json, LegacyUseTabSwitcherModeKey, _TabSwitcherMode) || _fixupsAppliedDuringLoad;

    _fixupsAppliedDuringLoad = JsonUtils::GetValueForKey(json, LegacyInputServiceWarningKey, _InputServiceWarning) || _fixupsAppliedDuringLoad;
    _fixupsAppliedDuringLoad = JsonUtils::GetValueForKey(json, LegacyWarnAboutLargePasteKey, _WarnAboutLargePaste) || _fixupsAppliedDuringLoad;
    _fixupsAppliedDuringLoad = JsonUtils::GetValueForKey(json, LegacyWarnAboutMultiLinePasteKey, _WarnAboutMultiLinePaste) || _fixupsAppliedDuringLoad;
    _fixupsAppliedDuringLoad = JsonUtils::GetValueForKey(json, LegacyConfirmCloseAllTabsKey, _ConfirmCloseAllTabs) || _fixupsAppliedDuringLoad;

#define GLOBAL_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);       \
    _logSettingIfSet(jsonKey, _##name.has_value());

    MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_LAYER_JSON)
#undef GLOBAL_SETTINGS_LAYER_JSON

    // GH#11975 We only want to allow sensible values and prevent crashes, so we are clamping those values
    // We only want to assign if the value did change through clamping,
    // otherwise we could end up setting defaults that get persisted
    if (this->HasInitialCols())
    {
        this->InitialCols(std::clamp(this->InitialCols(), 1, 999));
    }
    if (this->HasInitialRows())
    {
        this->InitialRows(std::clamp(this->InitialRows(), 1, 999));
    }
    LayerActionsFrom(json, origin, true);

    // No need to update _fixupsAppliedDuringLoad here.
    // We already handle this in SettingsLoader::FixupUserSettings().
    JsonUtils::GetValueForKey(json, LegacyReloadEnvironmentVariablesKey, _legacyReloadEnvironmentVariables);
    if (json[LegacyReloadEnvironmentVariablesKey.data()])
    {
        _logSettingSet(LegacyReloadEnvironmentVariablesKey);
    }

    JsonUtils::GetValueForKey(json, LegacyForceVTInputKey, _legacyForceVTInput);
    if (json[LegacyForceVTInputKey.data()])
    {
        _logSettingSet(LegacyForceVTInputKey);
    }

    // GLOBAL_SETTINGS_LAYER_JSON above should have already loaded this value properly.
    // We just need to detect if the legacy value was used and mark it for fixup, if so.
    if (const auto firstWindowPreferenceValue = json[FirstWindowPreferenceKey.data()])
    {
        _fixupsAppliedDuringLoad |= firstWindowPreferenceValue == LegacyPersistedWindowLayout.data();
    }

    // Remove settings included in userDefaults
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2> userDefaultSettings{ { { "copyOnSelect", "false" },
                                                                                                         { "copyFormatting", "false" } } };
    for (const auto& [setting, val] : userDefaultSettings)
    {
        if (const auto settingJson{ json.find(&*setting.cbegin(), (&*setting.cbegin()) + setting.size()) })
        {
            if (settingJson->asString() == val)
            {
                // false positive!
                _changeLog.erase(std::string{ setting });
            }
        }
    }
}

void GlobalAppSettings::LayerActionsFrom(const Json::Value& json, const OriginTag origin, const bool withKeybindings)
{
    // we want to do the keybindings map after the actions map so that we overwrite any leftover keybindings
    // that might have existed in the first pass, in case the user did a partial update from legacy to modern
    static constexpr std::array bindingsKeys{ ActionsKey, KeybindingsKey };
    for (const auto& jsonKey : bindingsKeys)
    {
        if (auto bindings{ json[JsonKey(jsonKey)] })
        {
            auto warnings = _actionMap->LayerJson(bindings, origin, withKeybindings);

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
Json::Value GlobalAppSettings::ToJson()
{
    // These experimental options should be removed from the settings file if they're at their default value.
    // This prevents them from sticking around forever, even if the user was just experimenting with them.
    // One could consider this a workaround for the settings UI right now not having a "reset to default" button for these.
    if (_GraphicsAPI == Control::GraphicsAPI::Automatic)
    {
        _GraphicsAPI.reset();
    }
    if (_TextMeasurement == Control::TextMeasurement::Graphemes)
    {
        _TextMeasurement.reset();
    }
    if (_DefaultInputScope == Control::DefaultInputScope::Default)
    {
        _DefaultInputScope.reset();
    }

    if (_DisablePartialInvalidation == false)
    {
        _DisablePartialInvalidation.reset();
    }
    if (_SoftwareRendering == false)
    {
        _SoftwareRendering.reset();
    }

    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, DefaultProfileKey, _UnparsedDefaultProfile);

#define GLOBAL_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_TO_JSON)
#undef GLOBAL_SETTINGS_TO_JSON

    json[JsonKey(ActionsKey)] = _actionMap->ToJson();
    json[JsonKey(KeybindingsKey)] = _actionMap->KeyBindingsToJson();

    return json;
}

bool GlobalAppSettings::FixupsAppliedDuringLoad()
{
    return _fixupsAppliedDuringLoad || _actionMap->FixupsAppliedDuringLoad();
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
    return FirstWindowPreference() != FirstWindowPreference::DefaultProfile;
}

void GlobalAppSettings::ResolveMediaResources(const Model::MediaResourceResolver& resolver)
{
    _actionMap->ResolveMediaResourcesWithBasePath(SourceBasePath, resolver);
    if (_NewTabMenu)
    {
        for (const auto& entry : *_NewTabMenu)
        {
            if (const auto resolvable{ entry.try_as<IPathlessMediaResourceContainer>() })
            {
                resolvable->ResolveMediaResourcesWithBasePath(SourceBasePath, resolver);
            }
        }
    }
    for (auto& parent : _parents)
    {
        parent->ResolveMediaResources(resolver);
    }
}

void GlobalAppSettings::_logSettingSet(const std::string_view& setting)
{
    if (setting == "theme")
    {
        if (_Theme.has_value())
        {
            // ThemePair always has a Dark/Light value,
            // so we need to check if they were explicitly set
            if (_Theme->DarkName() == _Theme->LightName())
            {
                _changeLog.emplace(setting);
            }
            else
            {
                _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, "dark"));
                _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, "light"));
            }
        }
    }
    else if (setting == "newTabMenu")
    {
        if (_NewTabMenu.has_value())
        {
            for (const auto& entry : *_NewTabMenu)
            {
                std::string entryType;
                switch (entry.Type())
                {
                case NewTabMenuEntryType::Profile:
                    entryType = "profile";
                    break;
                case NewTabMenuEntryType::Separator:
                    entryType = "separator";
                    break;
                case NewTabMenuEntryType::Folder:
                    entryType = "folder";
                    break;
                case NewTabMenuEntryType::RemainingProfiles:
                    entryType = "remainingProfiles";
                    break;
                case NewTabMenuEntryType::MatchProfiles:
                    entryType = "matchProfiles";
                    break;
                case NewTabMenuEntryType::Action:
                    entryType = "action";
                    break;
                case NewTabMenuEntryType::Invalid:
                    // ignore invalid
                    continue;
                }
                _changeLog.emplace(fmt::format(FMT_COMPILE("{}.{}"), setting, entryType));
            }
        }
    }
    else
    {
        _changeLog.emplace(setting);
    }
}

void GlobalAppSettings::UpdateCommandID(const Model::Command& cmd, winrt::hstring newID)
{
    const auto oldID = cmd.ID();
    _actionMap->UpdateCommandID(cmd, newID);
    // newID might have been empty when this function was called, if so actionMap would have generated a new ID, use that
    newID = cmd.ID();
    if (_NewTabMenu)
    {
        // Recursive lambda function to look through all the new tab menu entries and update IDs accordingly
        std::function<void(const Model::NewTabMenuEntry&)> recursiveEntryIdUpdate;
        recursiveEntryIdUpdate = [&](const Model::NewTabMenuEntry& entry) {
            if (entry.Type() == NewTabMenuEntryType::Action)
            {
                if (const auto actionEntry{ entry.try_as<ActionEntry>() })
                {
                    if (actionEntry.ActionId() == oldID)
                    {
                        actionEntry.ActionId(newID);
                    }
                }
            }
            else if (entry.Type() == NewTabMenuEntryType::Folder)
            {
                if (const auto folderEntry{ entry.try_as<FolderEntry>() })
                {
                    for (const auto& nestedEntry : folderEntry.RawEntries())
                    {
                        recursiveEntryIdUpdate(nestedEntry);
                    }
                }
            }
        };

        for (const auto& entry : *_NewTabMenu)
        {
            recursiveEntryIdUpdate(entry);
        }
    }
}

void GlobalAppSettings::_logSettingIfSet(const std::string_view& setting, const bool isSet)
{
    if (isSet)
    {
        // Exclude some false positives from userDefaults.json
        const bool settingCopyFormattingToDefault = til::equals_insensitive_ascii(setting, "copyFormatting") && _CopyFormatting.has_value() && _CopyFormatting.value() == static_cast<Control::CopyFormat>(0);
        const bool settingNTMToDefault = til::equals_insensitive_ascii(setting, "newTabMenu") && _NewTabMenu.has_value() && _NewTabMenu->Size() == 1 && _NewTabMenu->GetAt(0).Type() == NewTabMenuEntryType::RemainingProfiles;
        if (!settingCopyFormattingToDefault && !settingNTMToDefault)
        {
            _logSettingSet(setting);
        }
    }
}

void GlobalAppSettings::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
{
    for (const auto& setting : _changeLog)
    {
        changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
    }
}
