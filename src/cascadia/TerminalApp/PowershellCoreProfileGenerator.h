/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
-

Abstract:
-
Author(s):
- Mike Griese - August 2019

--*/

#pragma once
// #include <winrt/Microsoft.Terminal.TerminalConnection.h>
// #include "GlobalAppSettings.h"
// #include "TerminalWarnings.h"
#include "IDynamicProfileGenerator.h"

// // fwdecl unittest classes
// namespace TerminalAppLocalTests
// {
//     class SettingsTests;
// }

namespace TerminalApp
{
    class PowershellCoreProfileGenerator;
};

// AHG I need the

// Maybe just don't have the dynamic profile generator set the GUIDs at all. So
// long as the dynamic profile generator generates unique _names_, we can
// generate good guids for it on our side, if the guids was {0}. I guess if te
// generator wants to set guds itself it _can_ but it doesn't need to.

// class TerminalApp::AppGuidProvider
// {
// public:
//     GUID GetNamespaceGuid(IDynamicProfileGenerator& generator);
//     GUID GetGuidForName(IDynamicProfileGenerator& generator, std::wstring& name);
// }

class TerminalApp::PowershellCoreProfileGenerator : public TerminalApp::IDynamicProfileGenerator
{
public:
    PowershellCoreProfileGenerator() = default;
    ~PowershellCoreProfileGenerator() = default;
    std::wstring GetNamespace() override;
    // {
    //     // return TERMINAL_PROFILE_NAMESPACE_GUID;

    //     // return L"Windows.Terminal.PowershellCore";

    //     // Right now the powershell core profile is generated with
    //     // name = "Powershell Core"
    //     // auto profileGuid{ Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(name))) };
    //     // Profile newProfile{ profileGuid };

    //     // However with our current interface, this won't work at all for migrating

    //     // We'll just have the built-in profiles return null for their
    //     // Namespace, and special-case that internally as
    //     // TERMINAL_PROFILE_NAMESPACE_GUID

    //     // This howeevr means we won't be able to disable wsl/pscore/azure
    //     // individually. Might need to handle internal dynamic generators
    //     // differently.
    //     // Lets leave that as a todo for now.
    //     // TODO: reconcile lecacy dynamic profile's guids, namespace guids, and suppressing them
    //     return nullptr;
    // }

    std::vector<TerminalApp::Profile> GenerateProfiles() override;
    // {
    //     std::vector<TerminalApp::Profile> profiles{};

    //     std::filesystem::path psCoreCmdline{};
    //     if (_isPowerShellCoreInstalled(psCoreCmdline))
    //     {
    //         // auto pwshProfile{ _CreateDefaultProfile(L"PowerShell Core") };
    //         TerminalApp::Profile pwshProfile{ 0 };

    //         newProfile.SetName(L"PowerShell Core");

    //         // std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    //         // iconPath.append(Utils::GuidToString(profileGuid));
    //         // iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);
    //         std::wstring iconPath{ L"ms-appx:///ProfileIcons/" };
    //         iconPath.append(LegacyGuid);
    //         iconPath.append(L".png");
    //         newProfile.SetIconPath(iconPath);

    //         pwshProfile.SetCommandline(std::move(psCoreCmdline));
    //         pwshProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    //         pwshProfile.SetColorScheme({ L"Campbell" });

    //         // If powershell core is installed, we'll use that as the default.
    //         // Otherwise, we'll use normal Windows Powershell as the default.
    //         profiles.emplace_back(pwshProfile);
    //     }

    //     return profiles;
    // }

private:
    static constexpr std::wstring_view LegacyGuid{ L"{574e775e-4f2a-5b96-ac1e-a2962a402336}" };

    // Function Description:
    // - Returns true if the user has installed PowerShell Core. This will check
    //   both %ProgramFiles% and %ProgramFiles(x86)%, and will return true if
    //   powershell core was installed in either location.
    // Arguments:
    // - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
    // Return Value:
    // - true iff powershell core (pwsh.exe) is present.
    static bool _isPowerShellCoreInstalled(std::filesystem::path& cmdline);
    // {
    //     return _isPowerShellCoreInstalledInPath(L"%ProgramFiles%", cmdline) ||
    //            _isPowerShellCoreInstalledInPath(L"%ProgramFiles(x86)%", cmdline);
    // }

    // Function Description:
    // - Returns true if the user has installed PowerShell Core.
    // Arguments:
    // - A string that contains an environment-variable string in the form: %variableName%.
    // - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
    // Return Value:
    // - true iff powershell core (pwsh.exe) is present in the given path
    static bool _isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline);
    // {
    //     std::wstring programFileEnvNulTerm{ programFileEnv };
    //     std::filesystem::path psCorePath{ wil::ExpandEnvironmentStringsW<std::wstring>(programFileEnvNulTerm.data()) };
    //     psCorePath /= L"PowerShell";
    //     if (std::filesystem::exists(psCorePath))
    //     {
    //         for (auto& p : std::filesystem::directory_iterator(psCorePath))
    //         {
    //             psCorePath = p.path();
    //             psCorePath /= L"pwsh.exe";
    //             if (std::filesystem::exists(psCorePath))
    //             {
    //                 cmdline = psCorePath;
    //                 return true;
    //             }
    //         }
    //     }
    //     return false;
    // }
};
