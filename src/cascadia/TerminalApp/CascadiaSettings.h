/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.h

Abstract:
- This class acts as the container for all app settings. It's composed of two
        parts: Globals, which are app-wide settings, and Profiles, which contain
        a set of settings that apply to a single instance of the terminal.
  Also contains the logic for serializing and deserializing this object.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "GlobalAppSettings.h"
#include "TerminalWarnings.h"
#include "Profile.h"
#include "IDynamicProfileGenerator.h"

// {2bde4a90-d05f-401c-9492-e40884ead1d8}
// uuidv5 properties: name format is UTF-16LE bytes
static constexpr GUID TERMINAL_PROFILE_NAMESPACE_GUID = { 0x2bde4a90, 0xd05f, 0x401c, { 0x94, 0x92, 0xe4, 0x8, 0x84, 0xea, 0xd1, 0xd8 } };

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ProfileTests;
    class ColorSchemeTests;
    class KeyBindingsTests;
}

namespace TerminalApp
{
    class CascadiaSettings;
};

class TerminalApp::CascadiaSettings final
{
public:
    CascadiaSettings();
    CascadiaSettings(const bool addDynamicProfiles);
    ~CascadiaSettings();

    static std::unique_ptr<CascadiaSettings> LoadDefaults();
    static std::unique_ptr<CascadiaSettings> LoadAll();
    void SaveAll() const;

    winrt::Microsoft::Terminal::Settings::TerminalSettings MakeSettings(std::optional<GUID> profileGuid) const;

    GlobalAppSettings& GlobalSettings();

    std::basic_string_view<Profile> GetProfiles() const noexcept;

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    Json::Value ToJson() const;
    static std::unique_ptr<CascadiaSettings> FromJson(const Json::Value& json);
    void LayerJson(const Json::Value& json);

    static std::wstring GetSettingsPath(const bool useRoamingPath = false);
    static std::wstring GetDefaultSettingsPath();

    const Profile* FindProfile(GUID profileGuid) const noexcept;

    void CreateDefaults();

    std::vector<TerminalApp::SettingsLoadWarnings>& GetWarnings();

private:
    GlobalAppSettings _globals;
    std::vector<Profile> _profiles;
    std::vector<TerminalApp::SettingsLoadWarnings> _warnings{};

    std::vector<std::unique_ptr<TerminalApp::IDynamicProfileGenerator>> _profileGenerators{};

    Json::Value _userSettings;
    Json::Value _defaultSettings;

    void _CreateDefaultProfiles();

    void _LayerOrCreateProfile(const Json::Value& profileJson);
    Profile* _FindMatchingProfile(const Json::Value& profileJson);
    void _LayerOrCreateColorScheme(const Json::Value& schemeJson);
    ColorScheme* _FindMatchingColorScheme(const Json::Value& schemeJson);
    void _LayerJsonString(std::string_view fileData, const bool isDefaultSettings);
    static const Json::Value& _GetProfilesJsonObject(const Json::Value& json);

    void _LoadDynamicProfiles();

    static bool _IsPackaged();
    static void _WriteSettings(const std::string_view content);
    static std::optional<std::string> _ReadUserSettings();
    static std::optional<std::string> _ReadFile(HANDLE hFile);

    void _ValidateSettings();
    void _ValidateProfilesExist();
    void _ValidateProfilesHaveGuid();
    void _ValidateDefaultProfileExists();
    void _ValidateNoDuplicateProfiles();
    void _ReorderProfilesToMatchUserSettingsOrder();
    void _RemoveHiddenProfiles();

    static bool _isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline);
    static bool _isPowerShellCoreInstalled(std::filesystem::path& cmdline);
    static void _AppendWslProfiles(std::vector<TerminalApp::Profile>& profileStorage);
    static Profile _CreateDefaultProfile(const std::wstring_view name);

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ProfileTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
    friend class TerminalAppLocalTests::KeyBindingsTests;
};
