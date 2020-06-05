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

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ProfileTests;
    class ColorSchemeTests;
    class KeyBindingsTests;
    class TabTests;
};
namespace TerminalAppUnitTests
{
    class DynamicProfileTests;
    class JsonTests;
};

namespace TerminalApp
{
    class CascadiaSettings;
};

class TerminalApp::CascadiaSettings final
{
public:
    CascadiaSettings();
    explicit CascadiaSettings(const bool addDynamicProfiles);

    static std::unique_ptr<CascadiaSettings> LoadDefaults();
    static std::unique_ptr<CascadiaSettings> LoadAll();
    static std::unique_ptr<CascadiaSettings> LoadUniversal();

    static const CascadiaSettings& GetCurrentAppSettings();

    std::tuple<GUID, winrt::Microsoft::Terminal::Settings::TerminalSettings> BuildSettings(const winrt::TerminalApp::NewTerminalArgs& newTerminalArgs) const;
    winrt::Microsoft::Terminal::Settings::TerminalSettings BuildSettings(GUID profileGuid) const;

    GlobalAppSettings& GlobalSettings();

    std::basic_string_view<Profile> GetProfiles() const noexcept;

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    static std::unique_ptr<CascadiaSettings> FromJson(const Json::Value& json);
    void LayerJson(const Json::Value& json);

    static std::filesystem::path GetSettingsPath();
    static std::filesystem::path GetDefaultSettingsPath();

    const Profile* FindProfile(GUID profileGuid) const noexcept;

    std::vector<TerminalApp::SettingsLoadWarnings>& GetWarnings();

private:
    GlobalAppSettings _globals;
    std::vector<Profile> _profiles;
    std::vector<TerminalApp::SettingsLoadWarnings> _warnings;

    std::vector<std::unique_ptr<TerminalApp::IDynamicProfileGenerator>> _profileGenerators;

    std::string _userSettingsString;
    Json::Value _userSettings;
    Json::Value _defaultSettings;
    Json::Value _userDefaultProfileSettings{ Json::Value::null };

    void _LayerOrCreateProfile(const Json::Value& profileJson);
    Profile* _FindMatchingProfile(const Json::Value& profileJson);
    void _LayerOrCreateColorScheme(const Json::Value& schemeJson);
    ColorScheme* _FindMatchingColorScheme(const Json::Value& schemeJson);
    void _ParseJsonString(std::string_view fileData, const bool isDefaultSettings);
    static const Json::Value& _GetProfilesJsonObject(const Json::Value& json);
    static const Json::Value& _GetDisabledProfileSourcesJsonObject(const Json::Value& json);
    bool _PrependSchemaDirective();
    bool _AppendDynamicProfilesToUserSettings();
    std::string _ApplyFirstRunChangesToSettingsTemplate(std::string_view settingsTemplate) const;

    void _ApplyDefaultsFromUserSettings();

    void _LoadDynamicProfiles();

    static bool _IsPackaged();
    static void _WriteSettings(const std::string_view content);
    static std::optional<std::string> _ReadUserSettings();
    static std::optional<std::string> _ReadFile(HANDLE hFile);

    std::optional<GUID> _GetProfileGuidByName(const std::wstring_view) const;
    std::optional<GUID> _GetProfileGuidByIndex(std::optional<int> index) const;
    GUID _GetProfileForArgs(const winrt::TerminalApp::NewTerminalArgs& newTerminalArgs) const;

    void _ValidateSettings();
    void _ValidateProfilesExist();
    void _ValidateProfilesHaveGuid();
    void _ValidateDefaultProfileExists();
    void _ValidateNoDuplicateProfiles();
    void _ResolveDefaultProfile();
    void _ReorderProfilesToMatchUserSettingsOrder();
    void _RemoveHiddenProfiles();
    void _ValidateAllSchemesExist();
    void _ValidateMediaResources();
    void _ValidateKeybindings();
    void _ValidateNoGlobalsKey();

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ProfileTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
    friend class TerminalAppLocalTests::KeyBindingsTests;
    friend class TerminalAppLocalTests::TabTests;
    friend class TerminalAppUnitTests::DynamicProfileTests;
    friend class TerminalAppUnitTests::JsonTests;
};
