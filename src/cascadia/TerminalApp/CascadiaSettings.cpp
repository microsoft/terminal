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

using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace Microsoft::Console;

// {2bde4a90-d05f-401c-9492-e40884ead1d8}
// uuidv5 properties: name format is UTF-16LE bytes
static constexpr GUID TERMINAL_PROFILE_NAMESPACE_GUID = { 0x2bde4a90, 0xd05f, 0x401c, { 0x94, 0x92, 0xe4, 0x8, 0x84, 0xea, 0xd1, 0xd8 } };

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_PATH{ L"ms-appx:///ProfileIcons/" };

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_EXTENSION{ L".png" };
static constexpr std::wstring_view DEFAULT_LINUX_ICON_GUID{ L"{9acb9455-ca41-5af7-950f-6bca1bc9722f}" };

CascadiaSettings::CascadiaSettings() :
    _globals{},
    _profiles{}
{
}

CascadiaSettings::~CascadiaSettings()
{
}

// Method Description:
// - Create a set of profiles to use as the "default" profiles when initializing
//   the terminal. Currently, we create two or three profiles:
//    * one for cmd.exe
//    * one for powershell.exe (inbox Windows Powershell)
//    * if Powershell Core (pwsh.exe) is installed, we'll create another for
//      Powershell Core.
void CascadiaSettings::_CreateDefaultProfiles()
{
    auto cmdProfile{ _CreateDefaultProfile(L"cmd") };
    cmdProfile.SetFontFace(L"Consolas");
    cmdProfile.SetCommandline(L"cmd.exe");
    cmdProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    cmdProfile.SetColorScheme({ L"Campbell" });
    cmdProfile.SetAcrylicOpacity(0.75);
    cmdProfile.SetUseAcrylic(true);

    auto powershellProfile{ _CreateDefaultProfile(L"Windows PowerShell") };
    powershellProfile.SetCommandline(L"powershell.exe");
    powershellProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    powershellProfile.SetColorScheme({ L"Campbell" });
    powershellProfile.SetDefaultBackground(POWERSHELL_BLUE);
    powershellProfile.SetUseAcrylic(false);

    // If the user has installed PowerShell Core, we add PowerShell Core as a default.
    // PowerShell Core default folder is "%PROGRAMFILES%\PowerShell\[Version]\".
    std::filesystem::path psCoreCmdline{};
    if (_isPowerShellCoreInstalled(psCoreCmdline))
    {
        auto pwshProfile{ _CreateDefaultProfile(L"PowerShell Core") };
        pwshProfile.SetCommandline(std::move(psCoreCmdline));
        pwshProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        pwshProfile.SetColorScheme({ L"Campbell" });

        // If powershell core is installed, we'll use that as the default.
        // Otherwise, we'll use normal Windows Powershell as the default.
        _profiles.emplace_back(pwshProfile);
        _globals.SetDefaultProfile(pwshProfile.GetGuid());
    }
    else
    {
        _globals.SetDefaultProfile(powershellProfile.GetGuid());
    }

    _profiles.emplace_back(powershellProfile);
    _profiles.emplace_back(cmdProfile);

    if (winrt::Microsoft::Terminal::TerminalConnection::AzureConnection::IsAzureConnectionAvailable())
    {
        auto azureCloudShellProfile{ _CreateDefaultProfile(L"Azure Cloud Shell") };
        azureCloudShellProfile.SetCommandline(L"Azure");
        azureCloudShellProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        azureCloudShellProfile.SetColorScheme({ L"Vintage" });
        azureCloudShellProfile.SetAcrylicOpacity(0.6);
        azureCloudShellProfile.SetUseAcrylic(true);
        azureCloudShellProfile.SetCloseOnExit(false);
        azureCloudShellProfile.SetConnectionType(AzureConnectionType);
        _profiles.emplace_back(azureCloudShellProfile);
    }

    try
    {
        _AppendWslProfiles(_profiles);
    }
    CATCH_LOG()
}

// Method Description:
// - Initialize this object with default color schemes, profiles, and keybindings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::CreateDefaults()
{
    _CreateDefaultProfiles();
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

// Function Description:
// - Returns true if the user has installed PowerShell Core. This will check
//   both %ProgramFiles% and %ProgramFiles(x86)%, and will return true if
//   powershell core was installed in either location.
// Arguments:
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true iff powershell core (pwsh.exe) is present.
bool CascadiaSettings::_isPowerShellCoreInstalled(std::filesystem::path& cmdline)
{
    return _isPowerShellCoreInstalledInPath(L"%ProgramFiles%", cmdline) ||
           _isPowerShellCoreInstalledInPath(L"%ProgramFiles(x86)%", cmdline);
}

// Function Description:
// - Returns true if the user has installed PowerShell Core.
// Arguments:
// - A string that contains an environment-variable string in the form: %variableName%.
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true iff powershell core (pwsh.exe) is present in the given path
bool CascadiaSettings::_isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline)
{
    std::wstring programFileEnvNulTerm{ programFileEnv };
    std::filesystem::path psCorePath{ wil::ExpandEnvironmentStringsW<std::wstring>(programFileEnvNulTerm.data()) };
    psCorePath /= L"PowerShell";
    if (std::filesystem::exists(psCorePath))
    {
        for (auto& p : std::filesystem::directory_iterator(psCorePath))
        {
            psCorePath = p.path();
            psCorePath /= L"pwsh.exe";
            if (std::filesystem::exists(psCorePath))
            {
                cmdline = psCorePath;
                return true;
            }
        }
    }
    return false;
}

// Function Description:
// - Adds all of the WSL profiles to the provided container.
// Arguments:
// - A ref to the profiles container where the WSL profiles are to be added
// Return Value:
// - <none>
void CascadiaSettings::_AppendWslProfiles(std::vector<TerminalApp::Profile>& profileStorage)
{
    wil::unique_handle readPipe;
    wil::unique_handle writePipe;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&readPipe, &writePipe, &sa, 0));
    STARTUPINFO si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe.get();
    si.hStdError = writePipe.get();
    wil::unique_process_information pi{};
    wil::unique_cotaskmem_string systemPath;
    THROW_IF_FAILED(wil::GetSystemDirectoryW(systemPath));
    std::wstring command(systemPath.get());
    command += L"\\wsl.exe --list";

    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
                                             const_cast<LPWSTR>(command.c_str()),
                                             nullptr,
                                             nullptr,
                                             TRUE,
                                             CREATE_NO_WINDOW,
                                             nullptr,
                                             nullptr,
                                             &si,
                                             &pi));
    switch (WaitForSingleObject(pi.hProcess, INFINITE))
    {
    case WAIT_OBJECT_0:
        break;
    case WAIT_ABANDONED:
    case WAIT_TIMEOUT:
        THROW_HR(ERROR_CHILD_NOT_COMPLETE);
    case WAIT_FAILED:
        THROW_LAST_ERROR();
    default:
        THROW_HR(ERROR_UNHANDLED_EXCEPTION);
    }
    DWORD exitCode;
    if (GetExitCodeProcess(pi.hProcess, &exitCode) == false)
    {
        THROW_HR(E_INVALIDARG);
    }
    else if (exitCode != 0)
    {
        return;
    }
    DWORD bytesAvailable;
    THROW_IF_WIN32_BOOL_FALSE(PeekNamedPipe(readPipe.get(), nullptr, NULL, nullptr, &bytesAvailable, nullptr));
    std::wfstream pipe{ _wfdopen(_open_osfhandle((intptr_t)readPipe.get(), _O_WTEXT | _O_RDONLY), L"r") };
    // don't worry about the handle returned from wfdOpen, readPipe handle is already managed by wil
    // and closing the file handle will cause an error.
    std::wstring wline;
    std::getline(pipe, wline); // remove the header from the output.
    while (pipe.tellp() < bytesAvailable)
    {
        std::getline(pipe, wline);
        std::wstringstream wlinestream(wline);
        if (wlinestream)
        {
            std::wstring distName;
            std::getline(wlinestream, distName, L'\r');
            size_t firstChar = distName.find_first_of(L"( ");
            // Some localizations don't have a space between the name and "(Default)"
            // https://github.com/microsoft/terminal/issues/1168#issuecomment-500187109
            if (firstChar < distName.size())
            {
                distName.resize(firstChar);
            }
            auto WSLDistro{ _CreateDefaultProfile(distName) };
            WSLDistro.SetCommandline(L"wsl.exe -d " + distName);
            WSLDistro.SetColorScheme({ L"Campbell" });
            WSLDistro.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
            std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
            iconPath.append(DEFAULT_LINUX_ICON_GUID);
            iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);
            WSLDistro.SetIconPath(iconPath);
            profileStorage.emplace_back(WSLDistro);
        }
    }
}

// Method Description:
// - Helper function for creating a skeleton default profile with a pre-populated
//   guid and name.
// Arguments:
// - name: the name of the new profile.
// Return Value:
// - A Profile, ready to be filled in
Profile CascadiaSettings::_CreateDefaultProfile(const std::wstring_view name)
{
    auto profileGuid{ Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(name))) };
    Profile newProfile{ profileGuid };

    newProfile.SetName(static_cast<std::wstring>(name));

    std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    iconPath.append(Utils::GuidToString(profileGuid));
    iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);

    newProfile.SetIconPath(iconPath);

    return newProfile;
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

    // Re-order profiles so that all profiles from the user's settings appear
    // before profiles that _weren't_ in the user profiles.
    _ReorderProfilesToMatchUserSettingsOrder();

    // Remove hidden profiles _after_ re-ordering. The re-ordering uses the raw
    // json, and will get confused if the profile isn't in the list.
    _RemoveHiddenProfiles();

    // Verify all profiles actually had a GUID specified, otherwise generate a
    // GUID for them. Make sure to do this before de-duping profiles and
    // checking that the default profile is set.
    _ValidateProfilesHaveGuid();

    // Then do some validation on the profiles. The order of these does not
    // terribly matter.
    _ValidateNoDuplicateProfiles();
    _ValidateDefaultProfileExists();

    // TODO:<future> ensure that all the profile's color scheme names are
    // actually the names of schemes we've parsed. If the scheme doesn't exist,
    // just use the hardcoded defaults

    // TODO:<future> ensure there's at least one key bound. Display a warning if
    // there's _NO_ keys bound to any actions. That's highly irregular, and
    // likely an indication of an error somehow.
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

    std::vector<size_t> indiciesToDelete{};

    std::set<GUID, GuidOrdering> uniqueGuids{};

    // Try collecting all the unique guids. If we ever encounter a guid that's
    // already in the set, then we need to delete that profile.
    for (int i = 0; i < _profiles.size(); i++)
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
    std::set<GUID, GuidOrdering> uniqueGuids{};
    std::deque<GUID> guidOrder{};

    auto collectGuids = [&](const auto& json) {
        for (auto profileJson : _GetProfilesJsonObject(json))
        {
            if (profileJson.isObject())
            {
                if (profileJson.isMember("guid"))
                {
                    auto guidJson{ profileJson["guid"] };
                    auto guid = Utils::GuidFromString(GetWstringFromJson(guidJson));

                    if (uniqueGuids.insert(guid).second)
                    {
                        guidOrder.push_back(guid);
                    }
                }
            }
        }
    };

    // Push all the userSettings profiles' GUIDS into the set
    collectGuids(_userSettings);

    // Push all the defaultSettings profiles' GUIDS into the set
    collectGuids(_defaultSettings);
    GuidEquality equals{};
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
    _profiles.erase(std::remove_if(_profiles.begin(),
                                   _profiles.end(),
                                   [](auto&& profile) { return profile.IsHidden(); }),
                    _profiles.end());
}
