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
#include "LibraryResources.h"

#include "PowershellCoreProfileGenerator.h"
#include "WslDistroGenerator.h"
#include "AzureCloudShellGenerator.h"

#include "CascadiaSettings.g.cpp"

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::Console;

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_PATH{ L"ms-appx:///ProfileIcons/" };

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_EXTENSION{ L".png" };
static constexpr std::wstring_view DEFAULT_LINUX_ICON_GUID{ L"{9acb9455-ca41-5af7-950f-6bca1bc9722f}" };

// make sure this matches defaults.json.
static constexpr std::wstring_view DEFAULT_WINDOWS_POWERSHELL_GUID{ L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}" };

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
CascadiaSettings::CascadiaSettings(const bool addDynamicProfiles) :
    _globals{ winrt::make_self<implementation::GlobalAppSettings>() },
    _allProfiles{ winrt::single_threaded_observable_vector<Model::Profile>() },
    _activeProfiles{ winrt::single_threaded_observable_vector<Model::Profile>() },
    _warnings{ winrt::single_threaded_vector<SettingsLoadWarnings>() },
    _deserializationErrorMessage{ L"" },
    _defaultTerminals{ winrt::single_threaded_observable_vector<Model::DefaultTerminal>() },
    _currentDefaultTerminal{ nullptr }
{
    if (addDynamicProfiles)
    {
        _profileGenerators.emplace_back(std::make_unique<PowershellCoreProfileGenerator>());
        _profileGenerators.emplace_back(std::make_unique<WslDistroGenerator>());
        _profileGenerators.emplace_back(std::make_unique<AzureCloudShellGenerator>());
    }
}

CascadiaSettings::CascadiaSettings(winrt::hstring json) :
    CascadiaSettings(false)
{
    const auto jsonString{ til::u16u8(json) };
    _ParseJsonString(jsonString, false);
    LayerJson(_userSettings);
    _ValidateSettings();
}

winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings CascadiaSettings::Copy() const
{
    // dynamic profile generators added by default
    auto settings{ winrt::make_self<CascadiaSettings>() };
    settings->_globals = _globals->Copy();
    for (auto warning : _warnings)
    {
        settings->_warnings.Append(warning);
    }
    settings->_loadError = _loadError;
    settings->_deserializationErrorMessage = _deserializationErrorMessage;
    settings->_userSettingsString = _userSettingsString;
    settings->_userSettings = _userSettings;
    settings->_defaultSettings = _defaultSettings;

    settings->_defaultTerminals = _defaultTerminals;
    settings->_currentDefaultTerminal = _currentDefaultTerminal;

    _CopyProfileInheritanceTree(settings);

    return *settings;
}

// Method Description:
// - Copies the inheritance tree for profiles and hooks them up to a clone CascadiaSettings
// Arguments:
// - cloneSettings: the CascadiaSettings we're copying the inheritance tree to
// Return Value:
// - <none>
void CascadiaSettings::_CopyProfileInheritanceTree(winrt::com_ptr<CascadiaSettings>& cloneSettings) const
{
    // Our profiles inheritance graph doesn't have a formal root.
    // However, if we create a dummy Profile, and set _profiles as the parent,
    //  we now have a root. So we'll do just that, then copy the inheritance graph
    //  from the dummyRoot.
    auto dummyRootSource{ winrt::make_self<Profile>() };
    for (const auto& profile : _allProfiles)
    {
        winrt::com_ptr<Profile> profileImpl;
        profileImpl.copy_from(winrt::get_self<Profile>(profile));
        Profile::InsertParentHelper(dummyRootSource, profileImpl);
    }

    auto dummyRootClone{ winrt::make_self<Profile>() };
    std::unordered_map<void*, winrt::com_ptr<Profile>> visited{};

    if (_userDefaultProfileSettings)
    {
        // profile.defaults must be saved to CascadiaSettings
        // So let's do that manually first, and add that to visited
        cloneSettings->_userDefaultProfileSettings = Profile::CopySettings(_userDefaultProfileSettings);
        visited[_userDefaultProfileSettings.get()] = cloneSettings->_userDefaultProfileSettings;
    }

    Profile::CloneInheritanceGraph(dummyRootSource, dummyRootClone, visited);

    // All of the parents of the dummy root clone are _profiles.
    // Get the parents and add them to the settings clone.
    const auto cloneParents{ dummyRootClone->Parents() };
    for (const auto& profile : cloneParents)
    {
        cloneSettings->_allProfiles.Append(*profile);
        if (!profile->Hidden())
        {
            cloneSettings->_activeProfiles.Append(*profile);
        }
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
winrt::Microsoft::Terminal::Settings::Model::Profile CascadiaSettings::FindProfile(winrt::guid profileGuid) const noexcept
{
    const winrt::guid guid{ profileGuid };
    for (const auto& profile : _allProfiles)
    {
        try
        {
            if (profile.Guid() == guid)
            {
                return profile;
            }
        }
        CATCH_LOG();
    }
    return nullptr;
}

// Method Description:
// - Returns an iterable collection of all of our Profiles.
// Arguments:
// - <none>
// Return Value:
// - an iterable collection of all of our Profiles.
IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Profile> CascadiaSettings::AllProfiles() const noexcept
{
    return _allProfiles;
}

// Method Description:
// - Returns an iterable collection of all of our non-hidden Profiles.
// Arguments:
// - <none>
// Return Value:
// - an iterable collection of all of our Profiles.
IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Profile> CascadiaSettings::ActiveProfiles() const noexcept
{
    return _activeProfiles;
}

// Method Description:
// - Returns the globally configured keybindings
// Arguments:
// - <none>
// Return Value:
// - the globally configured keybindings
winrt::Microsoft::Terminal::Settings::Model::ActionMap CascadiaSettings::ActionMap() const noexcept
{
    return _globals->ActionMap();
}

// Method Description:
// - Get a reference to our global settings
// Arguments:
// - <none>
// Return Value:
// - a reference to our global settings
winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings CascadiaSettings::GlobalSettings() const
{
    return *_globals;
}

// Method Description:
// - Get a reference to our profiles.defaults object
// Arguments:
// - <none>
// Return Value:
// - a reference to our profile.defaults object
winrt::Microsoft::Terminal::Settings::Model::Profile CascadiaSettings::ProfileDefaults() const
{
    return *_userDefaultProfileSettings;
}

// Method Description:
// - Create a new profile based off the default profile settings.
// Arguments:
// - <none>
// Return Value:
// - a reference to the new profile
winrt::Microsoft::Terminal::Settings::Model::Profile CascadiaSettings::CreateNewProfile()
{
    if (_allProfiles.Size() == std::numeric_limits<uint32_t>::max())
    {
        // Shouldn't really happen
        return nullptr;
    }

    winrt::hstring newName{};
    for (uint32_t candidateIndex = 0; candidateIndex < _allProfiles.Size() + 1; candidateIndex++)
    {
        // There is a theoretical unsigned integer wraparound, which is OK
        newName = fmt::format(L"Profile {}", _allProfiles.Size() + 1 + candidateIndex);
        if (std::none_of(begin(_allProfiles), end(_allProfiles), [&](auto&& profile) { return profile.Name() == newName; }))
        {
            break;
        }
    }

    auto newProfile{ _userDefaultProfileSettings->CreateChild() };
    newProfile->Name(newName);
    _allProfiles.Append(*newProfile);
    return *newProfile;
}

// Method Description:
// - Duplicate a new profile based off another profile's settings
// - This differs from Profile::Copy because it also copies over settings
//   that were not defined in the json (for example, settings that were
//   defined in one of the parents)
// - This will not duplicate settings that were defined in profiles.defaults
//   however, because we do not want the json blob generated from the new profile
//   to contain those settings
// Arguments:
// - source: the Profile object we are duplicating (must not be null)
// Return Value:
// - a reference to the new profile
winrt::Microsoft::Terminal::Settings::Model::Profile CascadiaSettings::DuplicateProfile(Model::Profile source)
{
    THROW_HR_IF_NULL(E_INVALIDARG, source);

    winrt::com_ptr<Profile> duplicated;
    if (_userDefaultProfileSettings)
    {
        duplicated = _userDefaultProfileSettings->CreateChild();
    }
    else
    {
        duplicated = winrt::make_self<Profile>();
    }
    _allProfiles.Append(*duplicated);

    if (!source.Hidden())
    {
        _activeProfiles.Append(*duplicated);
    }

    winrt::hstring newName{ fmt::format(L"{} ({})", source.Name(), RS_(L"CopySuffix")) };

    // Check if this name already exists and if so, append a number
    for (uint32_t candidateIndex = 0; candidateIndex < _allProfiles.Size() + 1; ++candidateIndex)
    {
        if (std::none_of(begin(_allProfiles), end(_allProfiles), [&](auto&& profile) { return profile.Name() == newName; }))
        {
            break;
        }
        // There is a theoretical unsigned integer wraparound, which is OK
        newName = fmt::format(L"{} ({} {})", source.Name(), RS_(L"CopySuffix"), candidateIndex + 2);
    }
    duplicated->Name(winrt::hstring(newName));

#define DUPLICATE_SETTING_MACRO(settingName)                                                                                                   \
    if (source.Has##settingName() ||                                                                                                           \
        (source.##settingName##OverrideSource() != nullptr && source.##settingName##OverrideSource().Origin() != OriginTag::ProfilesDefaults)) \
    {                                                                                                                                          \
        duplicated->##settingName(source.##settingName());                                                                                     \
    }

#define DUPLICATE_APPEARANCE_SETTING_MACRO(settingName)                                                                                                                                                \
    if (source.DefaultAppearance().Has##settingName() ||                                                                                                                                               \
        (source.DefaultAppearance().##settingName##OverrideSource() != nullptr && source.DefaultAppearance().##settingName##OverrideSource().SourceProfile().Origin() != OriginTag::ProfilesDefaults)) \
    {                                                                                                                                                                                                  \
        duplicated->DefaultAppearance().##settingName(source.DefaultAppearance().##settingName());                                                                                                     \
    }

    DUPLICATE_SETTING_MACRO(Hidden);
    DUPLICATE_SETTING_MACRO(Icon);
    DUPLICATE_SETTING_MACRO(CloseOnExit);
    DUPLICATE_SETTING_MACRO(TabTitle);
    DUPLICATE_SETTING_MACRO(TabColor);
    DUPLICATE_SETTING_MACRO(SuppressApplicationTitle);
    DUPLICATE_SETTING_MACRO(UseAcrylic);
    DUPLICATE_SETTING_MACRO(AcrylicOpacity);
    DUPLICATE_SETTING_MACRO(ScrollState);
    DUPLICATE_SETTING_MACRO(FontFace);
    DUPLICATE_SETTING_MACRO(FontSize);
    DUPLICATE_SETTING_MACRO(FontWeight);
    DUPLICATE_SETTING_MACRO(Padding);
    DUPLICATE_SETTING_MACRO(Commandline);
    DUPLICATE_SETTING_MACRO(StartingDirectory);
    DUPLICATE_SETTING_MACRO(AntialiasingMode);
    DUPLICATE_SETTING_MACRO(ForceFullRepaintRendering);
    DUPLICATE_SETTING_MACRO(SoftwareRendering);
    DUPLICATE_SETTING_MACRO(HistorySize);
    DUPLICATE_SETTING_MACRO(SnapOnInput);
    DUPLICATE_SETTING_MACRO(AltGrAliasing);
    DUPLICATE_SETTING_MACRO(BellStyle);

    DUPLICATE_APPEARANCE_SETTING_MACRO(ColorSchemeName);
    DUPLICATE_APPEARANCE_SETTING_MACRO(Foreground);
    DUPLICATE_APPEARANCE_SETTING_MACRO(Background);
    DUPLICATE_APPEARANCE_SETTING_MACRO(SelectionBackground);
    DUPLICATE_APPEARANCE_SETTING_MACRO(CursorColor);
    DUPLICATE_APPEARANCE_SETTING_MACRO(PixelShaderPath);
    DUPLICATE_APPEARANCE_SETTING_MACRO(BackgroundImagePath);
    DUPLICATE_APPEARANCE_SETTING_MACRO(BackgroundImageOpacity);
    DUPLICATE_APPEARANCE_SETTING_MACRO(BackgroundImageStretchMode);
    DUPLICATE_APPEARANCE_SETTING_MACRO(BackgroundImageAlignment);
    DUPLICATE_APPEARANCE_SETTING_MACRO(RetroTerminalEffect);
    DUPLICATE_APPEARANCE_SETTING_MACRO(CursorShape);
    DUPLICATE_APPEARANCE_SETTING_MACRO(CursorHeight);

    // UnfocusedAppearance is treated as a single setting,
    // but requires a little more legwork to duplicate properly
    if (source.HasUnfocusedAppearance() ||
        (source.UnfocusedAppearanceOverrideSource() != nullptr && source.UnfocusedAppearanceOverrideSource().Origin() != OriginTag::ProfilesDefaults))
    {
        // First, get a com_ptr to the source's unfocused appearance
        // We need this to be able to call CopyAppearance (it is alright to simply call CopyAppearance here
        // instead of needing a separate function like DuplicateAppearance since UnfocusedAppearance is treated
        // as a single setting)
        winrt::com_ptr<AppearanceConfig> sourceUnfocusedAppearanceImpl;
        sourceUnfocusedAppearanceImpl.copy_from(winrt::get_self<AppearanceConfig>(source.UnfocusedAppearance()));

        // Get a weak ref to the duplicate profile so we can provide a source profile to the new UnfocusedAppearance
        // we are about to create
        const auto weakRefToDuplicated = weak_ref<Model::Profile>(*duplicated);
        auto duplicatedUnfocusedAppearanceImpl = AppearanceConfig::CopyAppearance(sourceUnfocusedAppearanceImpl, weakRefToDuplicated);

        // Make sure to add the default appearance of the duplicated profile as a parent to the duplicate's UnfocusedAppearance
        winrt::com_ptr<AppearanceConfig> duplicatedDefaultAppearanceImpl;
        duplicatedDefaultAppearanceImpl.copy_from(winrt::get_self<AppearanceConfig>(duplicated->DefaultAppearance()));
        duplicatedUnfocusedAppearanceImpl->InsertParent(duplicatedDefaultAppearanceImpl);

        // Finally, set the duplicate's UnfocusedAppearance
        duplicated->UnfocusedAppearance(*duplicatedUnfocusedAppearanceImpl);
    }

    if (source.HasConnectionType())
    {
        duplicated->ConnectionType(source.ConnectionType());
    }

    return *duplicated;
}

// Method Description:
// - Gets our list of warnings we found during loading. These are things that we
//   knew were bad when we called `_ValidateSettings` last.
// Return Value:
// - a reference to our list of warnings.
IVectorView<winrt::Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> CascadiaSettings::Warnings()
{
    return _warnings.GetView();
}

void CascadiaSettings::ClearWarnings()
{
    _warnings.Clear();
}

void CascadiaSettings::AppendWarning(SettingsLoadWarnings warning)
{
    _warnings.Append(warning);
}

winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Settings::Model::SettingsLoadErrors> CascadiaSettings::GetLoadingError()
{
    return _loadError;
}

winrt::hstring CascadiaSettings::GetSerializationErrorMessage()
{
    return _deserializationErrorMessage;
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
    // Make sure to check that profiles exists at all first and foremost:
    _ValidateProfilesExist();

    // Re-order profiles so that all profiles from the user's settings appear
    // before profiles that _weren't_ in the user profiles.
    _ReorderProfilesToMatchUserSettingsOrder();

    // Remove hidden profiles _after_ re-ordering. The re-ordering uses the raw
    // json, and will get confused if the profile isn't in the list.
    _UpdateActiveProfiles();

    // Then do some validation on the profiles. The order of these does not
    // terribly matter.
    _ValidateNoDuplicateProfiles();

    // Resolve the default profile before we validate that it exists.
    _ResolveDefaultProfile();
    _ValidateDefaultProfileExists();

    // Ensure that all the profile's color scheme names are
    // actually the names of schemes we've parsed. If the scheme doesn't exist,
    // just use the hardcoded defaults
    _ValidateAllSchemesExist();

    // Ensure all profile's with specified images resources have valid file path.
    // This validates icons and background images.
    _ValidateMediaResources();

    // TODO:GH#2548 ensure there's at least one key bound. Display a warning if
    // there's _NO_ keys bound to any actions. That's highly irregular, and
    // likely an indication of an error somehow.

    // GH#3522 - With variable args to keybindings, it's possible that a user
    // set a keybinding without all the required args for an action. Display a
    // warning if an action didn't have a required arg.
    // This will also catch other keybinding warnings, like from GH#4239
    _ValidateKeybindings();

    _ValidateColorSchemesInCommands();

    _ValidateNoGlobalsKey();
}

// Method Description:
// - Checks if the settings contain profiles at all. As we'll need to have some
//   profiles at all, we'll throw an error if there aren't any profiles.
void CascadiaSettings::_ValidateProfilesExist()
{
    const bool hasProfiles = _allProfiles.Size() > 0;
    if (!hasProfiles)
    {
        // Throw an exception. This is an invalid state, and we want the app to
        // be able to gracefully use the default settings.

        // We can't add the warning to the list of warnings here, because this
        // object is not going to be returned at any point.

        throw SettingsException(Microsoft::Terminal::Settings::Model::SettingsLoadErrors::NoProfiles);
    }
}

// Method Description:
// - Resolves the "defaultProfile", which can be a profile name, to a GUID
//   and stores it back to the globals.
void CascadiaSettings::_ResolveDefaultProfile()
{
    const auto unparsedDefaultProfile{ GlobalSettings().UnparsedDefaultProfile() };
    if (!unparsedDefaultProfile.empty())
    {
        auto maybeParsedDefaultProfile{ _GetProfileGuidByName(unparsedDefaultProfile) };
        auto defaultProfileGuid{ til::coalesce_value(maybeParsedDefaultProfile, winrt::guid{}) };
        GlobalSettings().DefaultProfile(defaultProfileGuid);
    }
}

// Method Description:
// - Checks if the "defaultProfile" is set to one of the profiles we
//   actually have. If the value is unset, or the value is set to something that
//   doesn't exist in the list of profiles, we'll arbitrarily pick the first
//   profile to use temporarily as the default.
// - Appends a SettingsLoadWarnings::MissingDefaultProfile to our list of
//   warnings if we failed to find the default.
void CascadiaSettings::_ValidateDefaultProfileExists()
{
    const winrt::guid defaultProfileGuid{ GlobalSettings().DefaultProfile() };
    const bool nullDefaultProfile = defaultProfileGuid == winrt::guid{};
    bool defaultProfileNotInProfiles = true;
    for (const auto& profile : _allProfiles)
    {
        if (profile.Guid() == defaultProfileGuid)
        {
            defaultProfileNotInProfiles = false;
            break;
        }
    }

    if (nullDefaultProfile || defaultProfileNotInProfiles)
    {
        _warnings.Append(Microsoft::Terminal::Settings::Model::SettingsLoadWarnings::MissingDefaultProfile);
        // Use the first profile as the new default

        // _temporarily_ set the default profile to the first profile. Because
        // we're adding a warning, this settings change won't be re-serialized.
        GlobalSettings().DefaultProfile(_allProfiles.GetAt(0).Guid());
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

    std::vector<uint32_t> indicesToDelete;

    std::set<winrt::guid> uniqueGuids;

    // Try collecting all the unique guids. If we ever encounter a guid that's
    // already in the set, then we need to delete that profile.
    for (uint32_t i = 0; i < _allProfiles.Size(); i++)
    {
        if (!uniqueGuids.insert(_allProfiles.GetAt(i).Guid()).second)
        {
            foundDupe = true;
            indicesToDelete.push_back(i);
        }
    }

    // Remove all the duplicates we've marked
    // Walk backwards, so we don't accidentally shift any of the elements
    for (auto iter = indicesToDelete.rbegin(); iter != indicesToDelete.rend(); iter++)
    {
        _allProfiles.RemoveAt(*iter);
    }

    if (foundDupe)
    {
        _warnings.Append(Microsoft::Terminal::Settings::Model::SettingsLoadWarnings::DuplicateProfile);
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
    std::set<winrt::guid> uniqueGuids;
    std::deque<winrt::guid> guidOrder;

    auto collectGuids = [&](const auto& json) {
        for (auto profileJson : _GetProfilesJsonObject(json))
        {
            if (profileJson.isObject())
            {
                auto guid = implementation::Profile::GetGuidOrGenerateForJson(profileJson);
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
    std::equal_to<winrt::guid> equals;
    // Re-order the list of profiles to match that ordering
    // for (gIndex=0 -> uniqueGuids.size)
    //   pIndex = the pIndex of the profile with guid==guids[gIndex]
    //   profiles.swap(pIndex <-> gIndex)
    // This is O(N^2), which is kinda rough. I'm sure there's a better way
    for (uint32_t gIndex = 0; gIndex < guidOrder.size(); gIndex++)
    {
        const auto guid = guidOrder.at(gIndex);
        for (uint32_t pIndex = gIndex; pIndex < _allProfiles.Size(); pIndex++)
        {
            auto profileGuid = _allProfiles.GetAt(pIndex).Guid();
            if (equals(profileGuid, guid))
            {
                auto prof1 = _allProfiles.GetAt(pIndex);
                _allProfiles.SetAt(pIndex, _allProfiles.GetAt(gIndex));
                _allProfiles.SetAt(gIndex, prof1);
                break;
            }
        }
    }
}

// Method Description:
// - Updates the list of active profiles from the list of all profiles
// - If there are no active profiles (all profiles are hidden), throw a SettingsException
// - Does not set any warnings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_UpdateActiveProfiles()
{
    _activeProfiles.Clear();
    for (auto const& profile : _allProfiles)
    {
        if (!profile.Hidden())
        {
            _activeProfiles.Append(profile);
        }
    }

    // Ensure that we still have some profiles here. If we don't, then throw an
    // exception, so the app can use the defaults.
    const bool hasProfiles = _activeProfiles.Size() > 0;
    if (!hasProfiles)
    {
        // Throw an exception. This is an invalid state, and we want the app to
        // be able to gracefully use the default settings.
        throw SettingsException(SettingsLoadErrors::AllProfilesHidden);
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
    for (auto profile : _allProfiles)
    {
        const auto schemeName = profile.DefaultAppearance().ColorSchemeName();
        if (!_globals->ColorSchemes().HasKey(schemeName))
        {
            // Clear the user set color scheme. We'll just fallback instead.
            profile.DefaultAppearance().ClearColorSchemeName();
            foundInvalidScheme = true;
        }
        if (profile.UnfocusedAppearance())
        {
            const auto unfocusedSchemeName = profile.UnfocusedAppearance().ColorSchemeName();
            if (!_globals->ColorSchemes().HasKey(unfocusedSchemeName))
            {
                profile.UnfocusedAppearance().ClearColorSchemeName();
                foundInvalidScheme = true;
            }
        }
    }

    if (foundInvalidScheme)
    {
        _warnings.Append(SettingsLoadWarnings::UnknownColorScheme);
    }
}

// Method Description:
// - Ensures that all specified images resources (icons and background images) are valid URIs.
//   This does not verify that the icon or background image files are encoded as an image.
// Arguments:
// - <none>
// Return Value:
// - <none>
// - Appends a SettingsLoadWarnings::InvalidBackgroundImage to our list of warnings if
//   we find any invalid background images.
// - Appends a SettingsLoadWarnings::InvalidIconImage to our list of warnings if
//   we find any invalid icon images.
void CascadiaSettings::_ValidateMediaResources()
{
    bool invalidBackground{ false };
    bool invalidIcon{ false };

    for (auto profile : _allProfiles)
    {
        if (!profile.DefaultAppearance().BackgroundImagePath().empty())
        {
            // Attempt to convert the path to a URI, the ctor will throw if it's invalid/unparseable.
            // This covers file paths on the machine, app data, URLs, and other resource paths.
            try
            {
                winrt::Windows::Foundation::Uri imagePath{ profile.DefaultAppearance().ExpandedBackgroundImagePath() };
            }
            catch (...)
            {
                // reset background image path
                profile.DefaultAppearance().BackgroundImagePath(L"");
                invalidBackground = true;
            }
        }

        if (profile.UnfocusedAppearance())
        {
            if (!profile.UnfocusedAppearance().BackgroundImagePath().empty())
            {
                // Attempt to convert the path to a URI, the ctor will throw if it's invalid/unparseable.
                // This covers file paths on the machine, app data, URLs, and other resource paths.
                try
                {
                    winrt::Windows::Foundation::Uri imagePath{ profile.UnfocusedAppearance().ExpandedBackgroundImagePath() };
                }
                catch (...)
                {
                    // reset background image path
                    profile.UnfocusedAppearance().BackgroundImagePath(L"");
                    invalidBackground = true;
                }
            }
        }

        if (!profile.Icon().empty())
        {
            const auto iconPath{ wil::ExpandEnvironmentStringsW<std::wstring>(profile.Icon().c_str()) };
            try
            {
                winrt::Windows::Foundation::Uri imagePath{ iconPath };
            }
            catch (...)
            {
                // Anything longer than 2 wchar_t's _isn't_ an emoji or symbol,
                // so treat it as an invalid path.
                if (iconPath.size() > 2)
                {
                    // reset icon path
                    profile.Icon(L"");
                    invalidIcon = true;
                }
            }
        }
    }

    if (invalidBackground)
    {
        _warnings.Append(SettingsLoadWarnings::InvalidBackgroundImage);
    }

    if (invalidIcon)
    {
        _warnings.Append(SettingsLoadWarnings::InvalidIcon);
    }
}

// Method Description:
// - Helper to get the GUID of a profile, given an optional index and a possible
//   "profile" value to override that.
// - First, we'll try looking up the profile for the given index. This will
//   either get us the GUID of the Nth profile, or the GUID of the default
//   profile.
// - Then, if there was a Profile set in the NewTerminalArgs, we'll use that to
//   try and look the profile up by either GUID or name.
// Arguments:
// - index: if provided, the index in the list of profiles to get the GUID for.
//   If omitted, instead use the default profile's GUID
// - newTerminalArgs: An object that may contain a profile name or GUID to
//   actually use. If the Profile value is not a guid, we'll treat it as a name,
//   and attempt to look the profile up by name instead.
// Return Value:
// - the GUID of the profile corresponding to this combination of index and NewTerminalArgs
winrt::guid CascadiaSettings::GetProfileForArgs(const Model::NewTerminalArgs& newTerminalArgs) const
{
    std::optional<winrt::guid> profileByIndex, profileByName;
    if (newTerminalArgs)
    {
        if (newTerminalArgs.ProfileIndex() != nullptr)
        {
            profileByIndex = _GetProfileGuidByIndex(newTerminalArgs.ProfileIndex().Value());
        }

        profileByName = _GetProfileGuidByName(newTerminalArgs.Profile());
    }

    return til::coalesce_value(profileByName, profileByIndex, _globals->DefaultProfile());
}

// Method Description:
// - Helper to get the GUID of a profile given a name that could be a guid or an actual name.
// Arguments:
// - name: a guid string _or_ the name of a profile
// Return Value:
// - the GUID of the profile corresponding to this name
std::optional<winrt::guid> CascadiaSettings::_GetProfileGuidByName(const winrt::hstring name) const
try
{
    // First, try and parse the "name" as a GUID. If it's a
    // GUID, and the GUID of one of our profiles, then use that as the
    // profile GUID instead. If it's not, then try looking it up as a
    // name of a profile. If it's still not that, then just ignore it.
    if (!name.empty())
    {
        // Do a quick heuristic check - is the profile 38 chars long (the
        // length of a GUID string), and does it start with '{'? Because if
        // it doesn't, it's _definitely_ not a GUID.
        if (name.size() == 38 && name[0] == L'{')
        {
            const auto newGUID{ Utils::GuidFromString(static_cast<std::wstring>(name)) };
            if (FindProfile(newGUID))
            {
                return newGUID;
            }
        }

        // Here, we were unable to use the profile string as a GUID to
        // lookup a profile. Instead, try using the string to look the
        // Profile up by name.
        for (auto profile : _allProfiles)
        {
            if (profile.Name() == name)
            {
                return profile.Guid();
            }
        }
    }

    return std::nullopt;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return std::nullopt;
}

// Method Description:
// - Helper to find the profile GUID for a the profile at the given index in the
//   list of profiles. If no index is provided, this instead returns the default
//   profile's guid. This is used by the NewTabProfile<N> ShortcutActions to
//   create a tab for the Nth profile in the list of profiles.
// Arguments:
// - index: if provided, the index in the list of profiles to get the GUID for.
//   If omitted, instead return the default profile's GUID
// Return Value:
// - the Nth profile's GUID, or the default profile's GUID
std::optional<winrt::guid> CascadiaSettings::_GetProfileGuidByIndex(std::optional<int> index) const
{
    if (index)
    {
        const auto realIndex{ index.value() };
        // If we don't have that many profiles, then do nothing.
        if (realIndex >= 0 &&
            realIndex < gsl::narrow_cast<decltype(realIndex)>(_activeProfiles.Size()))
        {
            const auto& selectedProfile = _activeProfiles.GetAt(realIndex);
            return selectedProfile.Guid();
        }
    }
    return std::nullopt;
}

// Method Description:
// - If there were any warnings we generated while parsing the user's
//   keybindings, add them to the list of warnings here. If there were warnings
//   generated in this way, we'll add a AtLeastOneKeybindingWarning, which will
//   act as a header for the other warnings
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_ValidateKeybindings()
{
    auto keybindingWarnings = _globals->KeybindingsWarnings();

    if (!keybindingWarnings.empty())
    {
        _warnings.Append(SettingsLoadWarnings::AtLeastOneKeybindingWarning);
        for (auto warning : keybindingWarnings)
        {
            _warnings.Append(warning);
        }
    }
}

// Method Description:
// - Ensures that every "setColorScheme" command has a valid "color scheme" set.
// Arguments:
// - <none>
// Return Value:
// - <none>
// - Appends a SettingsLoadWarnings::InvalidColorSchemeInCmd to our list of warnings if
//   we find any command with an invalid color scheme.
void CascadiaSettings::_ValidateColorSchemesInCommands()
{
    bool foundInvalidScheme{ false };
    for (const auto& nameAndCmd : _globals->ActionMap().NameMap())
    {
        if (_HasInvalidColorScheme(nameAndCmd.Value()))
        {
            foundInvalidScheme = true;
            break;
        }
    }

    if (foundInvalidScheme)
    {
        _warnings.Append(SettingsLoadWarnings::InvalidColorSchemeInCmd);
    }
}

bool CascadiaSettings::_HasInvalidColorScheme(const Model::Command& command)
{
    bool invalid{ false };
    if (command.HasNestedCommands())
    {
        for (const auto& nested : command.NestedCommands())
        {
            if (_HasInvalidColorScheme(nested.Value()))
            {
                invalid = true;
                break;
            }
        }
    }
    else if (const auto& actionAndArgs = command.ActionAndArgs())
    {
        if (const auto& realArgs = actionAndArgs.Args().try_as<Model::SetColorSchemeArgs>())
        {
            auto cmdImpl{ winrt::get_self<Command>(command) };
            // no need to validate iterable commands on color schemes
            // they will be expanded to commands with a valid scheme name
            if (cmdImpl->IterateOn() != ExpandCommandType::ColorSchemes &&
                !_globals->ColorSchemes().HasKey(realArgs.SchemeName()))
            {
                invalid = true;
            }
        }
    }

    return invalid;
}

// Method Description:
// - Checks for the presence of the legacy "globals" key in the user's
//   settings.json. If this key is present, then they've probably got a pre-0.11
//   settings file that won't work as expected anymore. We should warn them
//   about that.
// Arguments:
// - <none>
// Return Value:
// - <none>
// - Appends a SettingsLoadWarnings::LegacyGlobalsProperty to our list of warnings if
//   we find any invalid background images.
void CascadiaSettings::_ValidateNoGlobalsKey()
{
    // use isMember here. If you use [], you're actually injecting "globals": null.
    if (_userSettings.isMember("globals"))
    {
        _warnings.Append(SettingsLoadWarnings::LegacyGlobalsProperty);
    }
}

// Method Description
// - Replaces known tokens DEFAULT_PROFILE, PRODUCT and VERSION in the settings template
//   with their expected values. DEFAULT_PROFILE is updated to match PowerShell Core's GUID
//   if such a profile is detected. If it isn't, it'll be set to Windows PowerShell's GUID.
// Arguments:
// - settingsTemplate: a settings template
// Return value:
// - The new settings string.
std::string CascadiaSettings::_ApplyFirstRunChangesToSettingsTemplate(std::string_view settingsTemplate) const
{
    // We're using replace_needle_in_haystack_inplace here, because it's more
    // efficient to iteratively modify a single string in-place than it is to
    // keep copying over the contents and modifying a copy (which
    // replace_needle_in_haystack would do).
    std::string finalSettings{ settingsTemplate };

    std::wstring defaultProfileGuid{ DEFAULT_WINDOWS_POWERSHELL_GUID };
    if (const auto psCoreProfileGuid{ _GetProfileGuidByName(hstring{ PowershellCoreProfileGenerator::GetPreferredPowershellProfileName() }) })
    {
        defaultProfileGuid = Utils::GuidToString(*psCoreProfileGuid);
    }

    til::replace_needle_in_haystack_inplace(finalSettings,
                                            "%DEFAULT_PROFILE%",
                                            til::u16u8(defaultProfileGuid));

    til::replace_needle_in_haystack_inplace(finalSettings,
                                            "%VERSION%",
                                            til::u16u8(ApplicationVersion()));
    til::replace_needle_in_haystack_inplace(finalSettings,
                                            "%PRODUCT%",
                                            til::u16u8(ApplicationDisplayName()));

    til::replace_needle_in_haystack_inplace(finalSettings,
                                            "%COMMAND_PROMPT_LOCALIZED_NAME%",
                                            RS_A(L"CommandPromptDisplayName"));

    return finalSettings;
}

// Method Description:
// - Lookup the color scheme for a given profile. If the profile doesn't exist,
//   or the scheme name listed in the profile doesn't correspond to a scheme,
//   this will return `nullptr`.
// Arguments:
// - profileGuid: the GUID of the profile to find the scheme for.
// Return Value:
// - a non-owning pointer to the scheme.
winrt::Microsoft::Terminal::Settings::Model::ColorScheme CascadiaSettings::GetColorSchemeForProfile(const winrt::guid profileGuid) const
{
    auto profile = FindProfile(profileGuid);
    if (!profile)
    {
        return nullptr;
    }
    const auto schemeName = profile.DefaultAppearance().ColorSchemeName();
    return _globals->ColorSchemes().TryLookup(schemeName);
}

// Method Description:
// - updates all references to that color scheme with the new name
// Arguments:
// - oldName: the original name for the color scheme
// - newName: the new name for the color scheme
// Return Value:
// - <none>
void CascadiaSettings::UpdateColorSchemeReferences(const hstring oldName, const hstring newName)
{
    // update profiles.defaults, if necessary
    if (_userDefaultProfileSettings &&
        _userDefaultProfileSettings->DefaultAppearance().HasColorSchemeName() &&
        _userDefaultProfileSettings->DefaultAppearance().ColorSchemeName() == oldName)
    {
        _userDefaultProfileSettings->DefaultAppearance().ColorSchemeName(newName);
    }

    // update all profiles referencing this color scheme
    for (const auto& profile : _allProfiles)
    {
        if (profile.DefaultAppearance().HasColorSchemeName() && profile.DefaultAppearance().ColorSchemeName() == oldName)
        {
            profile.DefaultAppearance().ColorSchemeName(newName);
        }

        if (profile.UnfocusedAppearance())
        {
            if (profile.UnfocusedAppearance().HasColorSchemeName() && profile.UnfocusedAppearance().ColorSchemeName() == oldName)
            {
                profile.UnfocusedAppearance().ColorSchemeName(newName);
            }
        }
    }
}

winrt::hstring CascadiaSettings::ApplicationDisplayName()
{
    try
    {
        const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
        return package.DisplayName();
    }
    CATCH_LOG();

    return RS_(L"ApplicationDisplayNameUnpackaged");
}

winrt::hstring CascadiaSettings::ApplicationVersion()
{
    try
    {
        const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
        const auto version{ package.Id().Version() };
        winrt::hstring formatted{ wil::str_printf<std::wstring>(L"%u.%u.%u.%u", version.Major, version.Minor, version.Build, version.Revision) };
        return formatted;
    }
    CATCH_LOG();

    // Try to get the version the old-fashioned way
    try
    {
        struct LocalizationInfo
        {
            WORD language, codepage;
        };
        // Use the current module instance handle for TerminalApp.dll, nullptr for WindowsTerminal.exe
        auto filename{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        auto size{ GetFileVersionInfoSizeExW(0, filename.c_str(), nullptr) };
        THROW_LAST_ERROR_IF(size == 0);
        auto versionBuffer{ std::make_unique<std::byte[]>(size) };
        THROW_IF_WIN32_BOOL_FALSE(GetFileVersionInfoExW(0, filename.c_str(), 0, size, versionBuffer.get()));

        // Get the list of Version localizations
        LocalizationInfo* pVarLocalization{ nullptr };
        UINT varLen{ 0 };
        THROW_IF_WIN32_BOOL_FALSE(VerQueryValueW(versionBuffer.get(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&pVarLocalization), &varLen));
        THROW_HR_IF(E_UNEXPECTED, varLen < sizeof(*pVarLocalization)); // there must be at least one translation

        // Get the product version from the localized version compartment
        // We're using String/ProductVersion here because our build pipeline puts more rich information in it (like the branch name)
        // than in the unlocalized numeric version fields.
        WCHAR* pProductVersion{ nullptr };
        UINT versionLen{ 0 };
        const auto localizedVersionName{ wil::str_printf<std::wstring>(L"\\StringFileInfo\\%04x%04x\\ProductVersion",
                                                                       pVarLocalization->language ? pVarLocalization->language : 0x0409, // well-known en-US LCID
                                                                       pVarLocalization->codepage) };
        THROW_IF_WIN32_BOOL_FALSE(VerQueryValueW(versionBuffer.get(), localizedVersionName.c_str(), reinterpret_cast<void**>(&pProductVersion), &versionLen));
        return { pProductVersion };
    }
    CATCH_LOG();

    return RS_(L"ApplicationVersionUnknown");
}

// Method Description:
// - Forces a refresh of all default terminal state
// Arguments:
// - <none>
// Return Value:
// - <none> - Updates internal state
void CascadiaSettings::RefreshDefaultTerminals()
{
    _defaultTerminals.Clear();

    for (const auto& term : Model::DefaultTerminal::Available())
    {
        _defaultTerminals.Append(term);
    }

    _currentDefaultTerminal = Model::DefaultTerminal::Current();
}

// Helper to do the version check
static bool _isOnBuildWithDefTerm() noexcept
{
    OSVERSIONINFOEXW osver{ 0 };
    osver.dwOSVersionInfoSize = sizeof(osver);
    osver.dwBuildNumber = 21359;

    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    return VerifyVersionInfoW(&osver, VER_BUILDNUMBER, dwlConditionMask);
}

// Method Description:
// - Determines if we're on an OS platform that supports
//   the default terminal handoff functionality.
// Arguments:
// - <none>
// Return Value:
// - True if OS supports default terminal. False otherwise.
bool CascadiaSettings::IsDefaultTerminalAvailable() noexcept
{
    // Cached on first use since the OS version shouldn't change while we're running.
    static bool isAvailable = _isOnBuildWithDefTerm();
    return isAvailable;
}

// Method Description:
// - Returns an iterable collection of all available terminals.
// Arguments:
// - <none>
// Return Value:
// - an iterable collection of all available terminals that could be the default.
IObservableVector<winrt::Microsoft::Terminal::Settings::Model::DefaultTerminal> CascadiaSettings::DefaultTerminals() const noexcept
{
    return _defaultTerminals;
}

// Method Description:
// - Returns the currently selected default terminal application
// Arguments:
// - <none>
// Return Value:
// - the selected default terminal application
winrt::Microsoft::Terminal::Settings::Model::DefaultTerminal CascadiaSettings::CurrentDefaultTerminal() const noexcept
{
    return _currentDefaultTerminal;
}

// Method Description:
// - Sets the current default terminal application
// Arguments:
// - terminal - Terminal from `DefaultTerminals` list to set as default
// Return Value:
// - <none>
void CascadiaSettings::CurrentDefaultTerminal(winrt::Microsoft::Terminal::Settings::Model::DefaultTerminal terminal)
{
    _currentDefaultTerminal = terminal;
}
