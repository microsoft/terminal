;
;
; // Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowSettings.h"
#include "../../types/inc/Utils.hpp"
#include "JsonUtils.h"
#include "KeyChordSerialization.h"

#include "WindowSettings.g.cpp"

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

// Method Description:
// - Iterates through the "newTabMenu" entries and for ProfileEntries resolves the "profile"
//   fields, which can be a profile name, to a GUID and stores it back.
// - It finds any "source" entries and finds all profiles generated by that source
// - Lastly, it finds any "remainingProfiles" entries and stores which profiles they
//   represent (those that were not resolved before). It adds a warning when
//   multiple of these entries are found.
void WindowSettings::ResolveNewTabMenuProfiles(
    const winrt::Windows::Foundation::Collections::IVectorView<Model::Profile>& activeProfiles)
{
    Model::RemainingProfilesEntry remainingProfilesEntry = nullptr;

    // The TerminalPage needs to know which profile has which profile ID. To prevent
    // continuous lookups in the _activeProfiles vector, we create a map <int, Profile>
    // to store these indices in-flight.
    auto remainingProfilesMap = std::map<int, Model::Profile>{};
    auto activeProfileCount = gsl::narrow_cast<int>(activeProfiles.Size());
    for (auto profileIndex = 0; profileIndex < activeProfileCount; profileIndex++)
    {
        remainingProfilesMap.emplace(profileIndex, activeProfiles.GetAt(profileIndex));
    }

    // We keep track of the "remaining profiles" - those that have not yet been resolved
    // in either a "profile" or "source" entry. They will possibly be assigned to a
    // "remainingProfiles" entry
    auto remainingProfiles = single_threaded_map(std::move(remainingProfilesMap));

    // We call a recursive helper function to process the entries
    auto entries = NewTabMenu();
    _resolveNewTabMenuProfilesSet(activeProfiles, entries, remainingProfiles, remainingProfilesEntry);

    // If a "remainingProfiles" entry has been found, assign to it the remaining profiles
    if (remainingProfilesEntry != nullptr)
    {
        remainingProfilesEntry.Profiles(remainingProfiles);
    }

    // If the configuration does not have a "newTabMenu" field, GlobalAppSettings
    // will return a default value containing just a "remainingProfiles" entry. However,
    // this value is regenerated on every "get" operation, so the effect of setting
    // the remaining profiles above will be undone. So only in the case that no custom
    // value is present in GlobalAppSettings, we will store the modified default value.
    if (!HasNewTabMenu())
    {
        NewTabMenu(entries);
    }
}

// Method Description:
// - Helper function that processes a set of tab menu entries and resolves any profile names
//   or source fields as necessary - see function above for a more detailed explanation.
void WindowSettings::_resolveNewTabMenuProfilesSet(
    const winrt::Windows::Foundation::Collections::IVectorView<Model::Profile>& activeProfiles,
    const IVector<Model::NewTabMenuEntry> entries,
    IMap<int, Model::Profile>& remainingProfilesMap,
    Model::RemainingProfilesEntry& remainingProfilesEntry) const
{
    if (entries == nullptr || entries.Size() == 0)
    {
        return;
    }

    for (const auto& entry : entries)
    {
        if (entry == nullptr)
        {
            continue;
        }

        switch (entry.Type())
        {
        // For a simple profile entry, the "profile" field can either be a name or a GUID. We
        // use the GetProfileByName function to resolve this name to a profile instance, then
        // find the index of that profile, and store this information in the entry.
        case NewTabMenuEntryType::Profile:
        {
            // We need to access the unresolved profile name, a field that is not exposed
            // in the projected class. So, we need to first obtain our implementation struct
            // instance, to access this field.
            const auto profileEntry{ winrt::get_self<implementation::ProfileEntry>(entry.as<Model::ProfileEntry>()) };

            // Find the profile by name
            // TODO! only cascadiasettings knows this
            const Model::Profile& profile = nullptr; // GetProfileByName(profileEntry->ProfileName());

            // If not found, or if the profile is hidden, skip it
            if (profile == nullptr || profile.Hidden())
            {
                profileEntry->Profile(nullptr); // override "default" profile
                break;
            }

            // Find the index of the resulting profile and store the result in the entry
            uint32_t profileIndex;
            activeProfiles.IndexOf(profile, profileIndex);

            profileEntry->Profile(profile);
            profileEntry->ProfileIndex(profileIndex);

            // Remove from remaining profiles list (map)
            remainingProfilesMap.TryRemove(profileIndex);

            break;
        }

        // For a remainingProfiles entry, we store it in the variable that is passed back to our caller,
        // except when that one has already been set (so we found a second/third/...) instance, which will
        // trigger a warning. We then ignore this entry.
        case NewTabMenuEntryType::RemainingProfiles:
        {
            if (remainingProfilesEntry != nullptr)
            {
                // TODO! only cascadiasettings knows this
                // _warnings.Append(SettingsLoadWarnings::DuplicateRemainingProfilesEntry);
            }
            else
            {
                remainingProfilesEntry = entry.as<Model::RemainingProfilesEntry>();
            }
            break;
        }

        // For a folder, we simply call this method recursively
        case NewTabMenuEntryType::Folder:
        {
            // We need to access the unfiltered entry list, a field that is not exposed
            // in the projected class. So, we need to first obtain our implementation struct
            // instance, to access this field.
            const auto folderEntry{ winrt::get_self<implementation::FolderEntry>(entry.as<Model::FolderEntry>()) };

            auto folderEntries = folderEntry->RawEntries();
            _resolveNewTabMenuProfilesSet(activeProfiles, folderEntries, remainingProfilesMap, remainingProfilesEntry);
            break;
        }

        // For a "matchProfiles" entry, we iterate through the list of all profiles and
        // find all those matching: generated by the same source, having the same name, or
        // having the same commandline. This can be expanded with regex support in the future.
        // We make sure that none of the matches are included in the "remaining profiles" section.
        case NewTabMenuEntryType::MatchProfiles:
        {
            // We need to access the matching function, which is not exposed in the projected class.
            // So, we need to first obtain our implementation struct instance, to access this field.
            const auto matchEntry{ winrt::get_self<implementation::MatchProfilesEntry>(entry.as<Model::MatchProfilesEntry>()) };

            matchEntry->Profiles(single_threaded_map<int, Model::Profile>());

            auto activeProfileCount = gsl::narrow_cast<int>(activeProfiles.Size());
            for (auto profileIndex = 0; profileIndex < activeProfileCount; profileIndex++)
            {
                const auto profile = activeProfiles.GetAt(profileIndex);

                // On a match, we store it in the entry and remove it from the remaining list
                if (matchEntry->MatchesProfile(profile))
                {
                    matchEntry->Profiles().Insert(profileIndex, profile);
                    remainingProfilesMap.TryRemove(profileIndex);
                }
            }

            break;
        }
        }
    }
}
