/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.hpp

Abstract:
- This class acts as the container for all app settings. It's composed of two
        parts: Globals, which are app-wide settings, and Profiles, which contain
        a set of settings that apply to a single instance of the terminal.
  Also contains the logic for serializing and deserializing this object.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "GlobalAppSettings.h"
#include "Profile.h"


namespace TerminalApp
{
    class CascadiaSettings;
};

class TerminalApp::CascadiaSettings final
{

public:
    CascadiaSettings();
    ~CascadiaSettings();

    static std::unique_ptr<CascadiaSettings> LoadAll(const bool saveOnLoad = true);
    void SaveAll() const;

    winrt::Microsoft::Terminal::Settings::TerminalSettings MakeSettings(std::optional<GUID> profileGuid) const;

    GlobalAppSettings& GlobalSettings();

    std::basic_string_view<Profile> GetProfiles() const noexcept;

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    winrt::Windows::Data::Json::JsonObject ToJson() const;
    static std::unique_ptr<CascadiaSettings> FromJson(winrt::Windows::Data::Json::JsonObject json);

    static winrt::hstring GetSettingsPath();

    const Profile* FindProfile(GUID profileGuid) const noexcept;

    void CreateDefaults();
private:
    GlobalAppSettings _globals;
    std::vector<Profile> _profiles;


    void _CreateDefaultKeybindings();
    void _CreateDefaultSchemes();
    void _CreateDefaultProfiles();

    static bool _IsPackaged();
    static void _SaveAsPackagedApp(const winrt::hstring content);
    static void _SaveAsUnpackagedApp(const winrt::hstring content);
    static std::wstring _GetFullPathToUnpackagedSettingsFile();
    static winrt::hstring _GetPackagedSettingsPath();
    static std::optional<winrt::hstring> _LoadAsPackagedApp();
    static std::optional<winrt::hstring> _LoadAsUnpackagedApp();
    static bool _IsPowerShellCoreInstalled(std::wstring_view programFileEnv, std::filesystem::path& cmdline);
    static std::wstring ExpandEnvironmentVariableString(std::wstring_view source);
};
