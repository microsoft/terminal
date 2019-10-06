// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellCoreProfileGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

namespace
{
    struct PowerShellInstance
    {
        int majorVersion;
        bool preview;
        bool nativeArchitecture;
        std::filesystem::path rootPath;

        // Total sort order:
        // 6-preview (wow) < 6-preview (native) < 7-preview (native) < 6 (wow) < 6 (native) < 7 (native)
        bool operator<(const PowerShellInstance& second) const
        {
            if (preview != second.preview)
            {
                return preview; // preview is less than GA
            }
            if (majorVersion != second.majorVersion)
            {
                return majorVersion < second.majorVersion;
            }
            if (nativeArchitecture != second.nativeArchitecture)
            {
                return !nativeArchitecture; // non-native architecture is less than native architecture.
            }
            return rootPath < second.rootPath; // fall back to path sorting
        }
    };
}

using namespace ::TerminalApp;

static void _collectPowerShellInstancesInDirectory(std::wstring_view directory, std::vector<PowerShellInstance>& out)
{
    std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(directory.data()) };
    if (std::filesystem::exists(root))
    {
        for (auto& versionedDir : std::filesystem::directory_iterator(root))
        {
            auto versionedPath = versionedDir.path();
            auto executable = versionedPath / L"pwsh.exe";
            if (std::filesystem::exists(executable))
            {
                auto preview = versionedPath.filename().wstring().find(L"-preview") != std::wstring::npos;
                out.emplace_back(PowerShellInstance{ std::stoi(versionedPath.filename()), preview, true, versionedPath });
            }
        }
    }
}

static std::vector<PowerShellInstance> _enumeratePowerShellInstances()
{
    std::vector<PowerShellInstance> versions;
    _collectPowerShellInstancesInDirectory(L"%ProgramFiles(x86)%\\PowerShell", versions);
    for (auto& v : versions)
    {
        // If they came from ProgramFiles(x86), they're not the native architecture.
        v.nativeArchitecture = false;
    }
    _collectPowerShellInstancesInDirectory(L"%ProgramFiles%\\PowerShell", versions);
    std::sort(versions.rbegin(), versions.rend()); // sort in reverse (best first)
    return versions;
}

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
    auto psInstances = _enumeratePowerShellInstances();
    for (const auto& psI : psInstances)
    {
        std::wstring name = L"PowerShell";
        if (psI.majorVersion < 7)
        {
            name += L" Core";
        }
        name += L" " + std::to_wstring(psI.majorVersion);
        if (psI.preview)
        {
            name += L"-preview";
        }
        if (!psI.nativeArchitecture)
        {
            name += L" (x86)";
        }

        auto profile{ CreateDefaultProfile(name) };
        profile.SetCommandline((psI.rootPath / L"pwsh.exe").wstring());
        profile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        profile.SetColorScheme({ L"Campbell" });
        profile.SetIconPath((psI.rootPath / L"assets" / (psI.preview ? L"Powershell_av_colors.ico" : L"Powershell_black.ico")).wstring());
        profiles.emplace_back(std::move(profile));
    }

    if (profiles.size() > 0)
    {
        // Give the first ("best") profile the official "PowerShell Core" GUID.
        // This will turn the anchored default profile into "PowerShell Core Latest Non-Preview for Native Architecture"
        // (or the closest approximation thereof).
        auto firstProfile = profiles.begin();
        firstProfile->SetGuid(Microsoft::Console::Utils::GuidFromString(L"{574e775e-4f2a-5b96-ac1e-a2962a402336}"));
    }

    return profiles;
}
