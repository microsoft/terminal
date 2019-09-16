// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellCoreProfileGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

using namespace ::TerminalApp;

// Legacy GUIDs:
//   - PowerShell Core       574e775e-4f2a-5b96-ac1e-a2962a402336

std::wstring_view PowershellCoreProfileGenerator::GetNamespace()
{
    return PowershellCoreGeneratorNamespace;
}

// Method Description:
// - Checks if pwsh is installed, and if it is, creates a profile to launch it.
// Arguments:
// - <none>
// Return Value:
// - a vector with the PowerShell Core profile, if available.
std::vector<TerminalApp::Profile> PowershellCoreProfileGenerator::GenerateProfiles()
{
    std::vector<TerminalApp::Profile> profiles;

    std::filesystem::path psCoreCmdline;
    if (_isPowerShellCoreInstalled(psCoreCmdline))
    {
        auto pwshProfile{ CreateDefaultProfile(L"PowerShell Core") };

        pwshProfile.SetCommandline(std::move(psCoreCmdline));
        pwshProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        pwshProfile.SetColorScheme({ L"Campbell" });

        // If powershell core is installed, we'll use that as the default.
        // Otherwise, we'll use normal Windows Powershell as the default.
        profiles.emplace_back(pwshProfile);
    }

    return profiles;
}

// Function Description:
// - Returns true if the user has installed PowerShell Core. This will check
//   both %ProgramFiles% and %ProgramFiles(x86)%, and will return true if
//   powershell core was installed in either location.
// Arguments:
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true iff powershell core (pwsh.exe) is present.
bool PowershellCoreProfileGenerator::_isPowerShellCoreInstalled(std::filesystem::path& cmdline)
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
bool PowershellCoreProfileGenerator::_isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline)
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
