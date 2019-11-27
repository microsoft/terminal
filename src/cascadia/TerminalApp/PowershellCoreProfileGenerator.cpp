// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellCoreProfileGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

#include <appmodel.h>
#include <shlobj.h>

namespace
{
    struct PowerShellInstance
    {
        int majorVersion;
        bool preview;
        bool nativeArchitecture;
        bool store;
        std::filesystem::path executablePath;

        // Total sort order:
        // 6-preview (wow) < 6-preview (native) < 7-preview (native) < 6 (wow) < 6 (native) < 7 (native)
        bool operator<(const PowerShellInstance& second) const
        {
            if (majorVersion != second.majorVersion)
            {
                return majorVersion < second.majorVersion;
            }
            if (preview != second.preview)
            {
                return preview; // preview is less than GA
            }
            if (nativeArchitecture != second.nativeArchitecture)
            {
                return !nativeArchitecture; // non-native architecture is less than native architecture.
            }
            if (store != second.store)
            {
                return !store; // non-store is less than store.
            }
            return executablePath < second.executablePath; // fall back to path sorting
        }
    };
}

using namespace ::TerminalApp;

static void _collectTraditionalLayoutPowerShellInstancesInDirectory(std::wstring_view directory, std::vector<PowerShellInstance>& out)
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
                out.emplace_back(PowerShellInstance{ std::stoi(versionedPath.filename()), preview, true, false, executable });
            }
        }
    }
}

static void _collectStorePowerShellInstances(std::vector<PowerShellInstance>& out)
{
    wil::unique_cotaskmem_string localAppDataFolder;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &localAppDataFolder)))
    {
        return;
    }

    std::filesystem::path appExecAliasPath{ localAppDataFolder.get() };
    appExecAliasPath /= L"Microsoft";
    appExecAliasPath /= L"WindowsApps";

    if (std::filesystem::exists(appExecAliasPath))
    {
        auto previewPath = appExecAliasPath / L"Microsoft.PowerShellPreview_8wekyb3d8bbwe";
        auto gaPath = appExecAliasPath / L"Microsoft.PowerShell_8wekyb3d8bbwe";
        if (std::filesystem::exists(previewPath))
        {
                out.emplace_back(PowerShellInstance{ 10, true, true, true, previewPath / L"pwsh.exe" });
        }
        if (std::filesystem::exists(gaPath))
        {
                out.emplace_back(PowerShellInstance{ 10, false, true, true, gaPath / L"pwsh.exe" });
        }
    }
}

static std::vector<PowerShellInstance> _enumeratePowerShellInstances()
{
    std::vector<PowerShellInstance> versions;
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles(x86)%\\PowerShell", versions);
    for (auto& v : versions)
    {
        // If they came from ProgramFiles(x86), they're not the native architecture.
        v.nativeArchitecture = false;
    }
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles%\\PowerShell", versions);
    _collectStorePowerShellInstances(versions);
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
        if (psI.store)
        {
            name += L" (Store)";
        }
        if (!psI.nativeArchitecture)
        {
            name += L" (x86)";
        }

        auto profile{ CreateDefaultProfile(name) };
        profile.SetCommandline(psI.executablePath.wstring());
        profile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        profile.SetColorScheme({ L"Campbell" });

        //profile.SetIconPath((psI.executablePath / L"assets" / (psI.preview ? L"Powershell_av_colors.ico" : L"Powershell_black.ico")).wstring());
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
