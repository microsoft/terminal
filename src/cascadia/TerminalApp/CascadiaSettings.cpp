// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include <conattrs.hpp>
#include <io.h>
#include <fcntl.h>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"

#include "PowershellCoreProfileGenerator.h"
#include "WslDistroGenerator.h"
#include "AzureCloudShellGenerator.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace Microsoft::Console;

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_PATH{ L"ms-appx:///ProfileIcons/" };

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_EXTENSION{ L".png" };
static constexpr std::wstring_view DEFAULT_LINUX_ICON_GUID{ L"{9acb9455-ca41-5af7-950f-6bca1bc9722f}" };

CascadiaSettings::CascadiaSettings() :
    CascadiaSettings(true)
{
}

// Constructor Description:
// - Creates a new settings object. If addDynamicProfiles is true, we'll
//   automatically add the built-in profile generators to our list of profile
//   generators. Set this to `false` for unit testing.
// Arguments:
// - addDynamicProfiles: if true, we'll add the built-in DPGs.
CascadiaSettings::CascadiaSettings(const bool addDynamicProfiles)
{
    if (addDynamicProfiles)
    {
        _profileGenerators.emplace_back(std::make_unique<PowershellCoreProfileGenerator>());
        _profileGenerators.emplace_back(std::make_unique<WslDistroGenerator>());
        _profileGenerators.emplace_back(std::make_unique<AzureCloudShellGenerator>());
    }
}

// Method Description:
// - Finds a profile that matches the given GUID. If there is no profile in this
//      settings object that matches, returns nullptr.
// Arguments:
// - profileGuid: the GUID of the profile to return.
// Return Value:
// - a non-ownership pointer to the profile matching the given guid, or nullptr
//      if there is no match.
const Profile* CascadiaSettings::FindProfile(GUID profileGuid) const noexcept
{
    for (auto& profile : _profiles)
    {
        if (profile.GetGuid() == profileGuid)
        {
            return &profile;
        }
    }
    return nullptr;
}

// Method Description:
// - Create a TerminalSettings object from the given profile.
//      If the profileGuidArg is not provided, this method will use the default
//      profile.
//   The TerminalSettings object that is created can be used to initialize both
//      the Control's settings, and the Core settings of the terminal.
// Arguments:
// - profileGuidArg: an optional GUID to use to lookup the profile to create the
//      settings from. If this arg is not provided, or the GUID does not match a
//       profile, then this method will use the default profile.
// Return Value:
// - <none>
TerminalSettings CascadiaSettings::MakeSettings(std::optional<GUID> profileGuidArg) const
{
    GUID profileGuid = profileGuidArg ? profileGuidArg.value() : _globals.GetDefaultProfile();
    const Profile* const profile = FindProfile(profileGuid);
    if (profile == nullptr)
    {
        throw E_INVALIDARG;
    }

    TerminalSettings result = profile->CreateTerminalSettings(_globals.GetColorSchemes());

    // Place our appropriate global settings into the Terminal Settings
    _globals.ApplyToSettings(result);

    return result;
}

// Method Description:
// - Returns an iterable collection of all of our Profiles.
// Arguments:
// - <none>
// Return Value:
// - an iterable collection of all of our Profiles.
std::basic_string_view<Profile> CascadiaSettings::GetProfiles() const noexcept
{
    return { &_profiles[0], _profiles.size() };
}

// Method Description:
// - Returns the globally configured keybindings
// Arguments:
// - <none>
// Return Value:
// - the globally configured keybindings
AppKeyBindings CascadiaSettings::GetKeybindings() const noexcept
{
    return _globals.GetKeybindings();
}

// Method Description:
// - Get a reference to our global settings
// Arguments:
// - <none>
// Return Value:
// - a reference to our global settings
GlobalAppSettings& CascadiaSettings::GlobalSettings()
{
    return _globals;
}

// Method Description:
// - Gets our list of warnings we found during loading. These are things that we
//   knew were bad when we called `_ValidateSettings` last.
// Return Value:
// - a reference to our list of warnings.
std::vector<TerminalApp::SettingsLoadWarnings>& CascadiaSettings::GetWarnings()
{
    return _warnings;
}

// Method Description:
// - Attempts to validate this settings structure. If there are critical errors
//   found, they'll be thrown as a SettingsLoadError. Non-critical errors, such
//   as not finding the default profile, will only result in an error. We'll add
//   all these warnings to our list of warnings, and the application can chose
//   to display these to the user.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_ValidateSettings()
{
    _warnings.clear();

    // Make sure to check that profiles exists at all first and foremost:
    _ValidateProfilesExist();

    // Verify all profiles actually had a GUID specified, otherwise generate a
    // GUID for them. Make sure to do this before de-duping profiles and
    // checking that the default profile is set.
    _ValidateProfilesHaveGuid();

    // Re-order profiles so that all profiles from the user's settings appear
    // before profiles that _weren't_ in the user profiles.
    _ReorderProfilesToMatchUserSettingsOrder();

    // Remove hidden profiles _after_ re-ordering. The re-ordering uses the raw
    // json, and will get confused if the profile isn't in the list.
    _RemoveHiddenProfiles();

    // Then do some validation on the profiles. The order of these does not
    // terribly matter.
    _ValidateNoDuplicateProfiles();
    _ValidateDefaultProfileExists();

    // Ensure that all the profile's color scheme names are
    // actually the names of schemes we've parsed. If the scheme doesn't exist,
    // just use the hardcoded defaults
    _ValidateAllSchemesExist();

    // TODO:GH#2548 ensure there's at least one key bound. Display a warning if
    // there's _NO_ keys bound to any actions. That's highly irregular, and
    // likely an indication of an error somehow.

    // TODO:GH#3522 With variable args to keybindings, it's possible that a user
    // set a keybinding without all the required args for an action. Display a
    // warning if an action didn't have a required arg.
}

// Method Description:
// - Checks if the settings contain profiles at all. As we'll need to have some
//   profiles at all, we'll throw an error if there aren't any profiles.
void CascadiaSettings::_ValidateProfilesExist()
{
    const bool hasProfiles = !_profiles.empty();
    if (!hasProfiles)
    {
        // Throw an exception. This is an invalid state, and we want the app to
        // be able to gracefully use the default settings.

        // We can't add the warning to the list of warnings here, because this
        // object is not going to be returned at any point.

        throw ::TerminalApp::SettingsException(::TerminalApp::SettingsLoadErrors::NoProfiles);
    }
}

// Method Description:
// - Walks through each profile, and ensures that they had a GUID set at some
//   point. If the profile did _not_ have a GUID ever set for it, generate a
//   temporary runtime GUID for it. This valitation does not add any warnnings.
void CascadiaSettings::_ValidateProfilesHaveGuid()
{
    for (auto& profile : _profiles)
    {
        profile.GenerateGuidIfNecessary();
    }
}

// Method Description:
// - Checks if the "globals.defaultProfile" is set to one of the profiles we
//   actually have. If the value is unset, or the value is set to something that
//   doesn't exist in the list of profiles, we'll arbitrarily pick the first
//   profile to use temporarily as the default.
// - Appends a SettingsLoadWarnings::MissingDefaultProfile to our list of
//   warnings if we failed to find the default.
void CascadiaSettings::_ValidateDefaultProfileExists()
{
    const auto defaultProfileGuid = GlobalSettings().GetDefaultProfile();
    const bool nullDefaultProfile = defaultProfileGuid == GUID{};
    bool defaultProfileNotInProfiles = true;
    for (const auto& profile : _profiles)
    {
        if (profile.GetGuid() == defaultProfileGuid)
        {
            defaultProfileNotInProfiles = false;
            break;
        }
    }

    if (nullDefaultProfile || defaultProfileNotInProfiles)
    {
        _warnings.push_back(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile);
        // Use the first profile as the new default

        // _temporarily_ set the default profile to the first profile. Because
        // we're adding a warning, this settings change won't be re-serialized.
        GlobalSettings().SetDefaultProfile(_profiles[0].GetGuid());
    }
}

// Method Description:
// - Checks to make sure there aren't any duplicate profiles in the list of
//   profiles. If so, we'll remove the subsequent entries (temporarily), as they
//   won't be accessible anyways.
// - Appends a SettingsLoadWarnings::DuplicateProfile to our list of warnings if
//   we find any such duplicate.
void CascadiaSettings::_ValidateNoDuplicateProfiles()
{
    bool foundDupe = false;

    std::vector<size_t> indiciesToDelete;

    std::set<GUID> uniqueGuids;

    // Try collecting all the unique guids. If we ever encounter a guid that's
    // already in the set, then we need to delete that profile.
    for (size_t i = 0; i < _profiles.size(); i++)
    {
        if (!uniqueGuids.insert(_profiles.at(i).GetGuid()).second)
        {
            foundDupe = true;
            indiciesToDelete.push_back(i);
        }
    }

    // Remove all the duplicates we've marked
    // Walk backwards, so we don't accidentally shift any of the elements
    for (auto iter = indiciesToDelete.rbegin(); iter != indiciesToDelete.rend(); iter++)
    {
        _profiles.erase(_profiles.begin() + *iter);
    }

    if (foundDupe)
    {
        _warnings.push_back(::TerminalApp::SettingsLoadWarnings::DuplicateProfile);
    }
}

// Method Description:
// - Re-orders the list of profiles to match what the user would expect them to
//   be. Orders profiles to be in the ordering { [profiles from user settings],
//   [default profiles that weren't in the user profiles]}.
// - Does not set any warnings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_ReorderProfilesToMatchUserSettingsOrder()
{
    std::set<GUID> uniqueGuids;
    std::deque<GUID> guidOrder;

    auto collectGuids = [&](const auto& json) {
        for (auto profileJson : _GetProfilesJsonObject(json))
        {
            if (profileJson.isObject())
            {
                auto guid = Profile::GetGuidOrGenerateForJson(profileJson);
                if (uniqueGuids.insert(guid).second)
                {
                    guidOrder.push_back(guid);
                }
            }
        }
    };

    // Push all the userSettings profiles' GUIDS into the set
    collectGuids(_userSettings);

    // Push all the defaultSettings profiles' GUIDS into the set
    collectGuids(_defaultSettings);
    std::equal_to<GUID> equals;
    // Re-order the list of _profiles to match that ordering
    // for (gIndex=0 -> uniqueGuids.size)
    //   pIndex = the pIndex of the profile with guid==guids[gIndex]
    //   profiles.swap(pIndex <-> gIndex)
    // This is O(N^2), which is kinda rough. I'm sure there's a better way
    for (size_t gIndex = 0; gIndex < guidOrder.size(); gIndex++)
    {
        const auto guid = guidOrder.at(gIndex);
        for (size_t pIndex = gIndex; pIndex < _profiles.size(); pIndex++)
        {
            auto profileGuid = _profiles.at(pIndex).GetGuid();
            if (equals(profileGuid, guid))
            {
                std::iter_swap(_profiles.begin() + pIndex, _profiles.begin() + gIndex);
                break;
            }
        }
    }
}

// Method Description:
// - Removes any profiles marked "hidden" from the list of profiles.
// - Does not set any warnings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_RemoveHiddenProfiles()
{
    // remove_if will move all the profiles where the lambda is true to the end
    // of the list, then return a iterator to the point in the list where those
    // profiles start. The erase call will then remove all of those profiles
    // from the list. This is the [erase-remove
    // idiom](https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom)
    _profiles.erase(std::remove_if(_profiles.begin(),
                                   _profiles.end(),
                                   [](auto&& profile) { return profile.IsHidden(); }),
                    _profiles.end());

    // Ensure that we still have some profiles here. If we don't, then throw an
    // exception, so the app can use the defaults.
    const bool hasProfiles = !_profiles.empty();
    if (!hasProfiles)
    {
        // Throw an exception. This is an invalid state, and we want the app to
        // be able to gracefully use the default settings.
        throw ::TerminalApp::SettingsException(::TerminalApp::SettingsLoadErrors::AllProfilesHidden);
    }
}

// Method Description:
// - Ensures that every profile has a valid "color scheme" set. If any profile
//   has a colorScheme set to a value which is _not_ the name of an actual color
//   scheme, we'll set the color table of the profile to something reasonable.
// Arguments:
// - <none>
// Return Value:
// - <none>
// - Appends a SettingsLoadWarnings::UnknownColorScheme to our list of warnings if
//   we find any such duplicate.
void CascadiaSettings::_ValidateAllSchemesExist()
{
    bool foundInvalidScheme = false;
    for (auto& profile : _profiles)
    {
        auto schemeName = profile.GetSchemeName();
        if (schemeName.has_value())
        {
            const auto found = _globals.GetColorSchemes().find(schemeName.value());
            if (found == _globals.GetColorSchemes().end())
            {
                profile.SetColorScheme({ L"Campbell" });
                foundInvalidScheme = true;
            }
        }
    }

    if (foundInvalidScheme)
    {
        _warnings.push_back(::TerminalApp::SettingsLoadWarnings::UnknownColorScheme);
    }
}
