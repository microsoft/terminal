// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellCoreProfileGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>

#include <appmodel.h>
#include <shlobj.h>

static constexpr std::wstring_view POWERSHELL_PFN{ L"Microsoft.PowerShell_8wekyb3d8bbwe" };
static constexpr std::wstring_view POWERSHELL_PREVIEW_PFN{ L"Microsoft.PowerShellPreview_8wekyb3d8bbwe" };
static constexpr std::wstring_view PWSH_EXE{ L"pwsh.exe" };
static constexpr std::wstring_view POWERSHELL_ICON{ L"ms-appx:///ProfileIcons/pwsh.ico" };
static constexpr std::wstring_view POWERSHELL_PREVIEW_ICON{ L"ms-appx:///ProfileIcons/pwsh-preview.ico" };

namespace
{
    enum PowerShellFlags
    {
        // these flags are used as a sort key, so they encode some native ordering
        Preview = 1 << 0, // preview version
        WOW = 1 << 1, // non-native (Windows-on-Windows)
        Store = 1 << 2, // distributed via the store
        Scoop = 1 << 3, // installed via Scoop
        Dotnet = 1 << 4, // installed as a dotnet global tool
    };
    DEFINE_ENUM_FLAG_OPERATORS(PowerShellFlags);

    struct PowerShellInstance
    {
        int majorVersion; // 0 = we don't know
        PowerShellFlags flags;
        std::filesystem::path executablePath;
        std::wstring iconPath; // not a std::filesystem::path because it could be a URI

        // Total sort order:
        // 6-preview (wow) < 6-preview (native) < 7-preview (native) < 6 (wow) < 6 (native) < 7 (native)
        bool operator<(const PowerShellInstance& second) const
        {
            if (majorVersion != second.majorVersion)
            {
                return majorVersion < second.majorVersion;
            }
            if (flags != second.flags)
            {
                return flags > second.flags; // flags are inverted (preview is worse than GA, WOW is worse than native, store is worse than traditional)
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
            auto executable = versionedPath / PWSH_EXE;
            if (std::filesystem::exists(executable))
            {
                auto preview = versionedPath.filename().wstring().find(L"-preview") != std::wstring::npos;
                PowerShellFlags flags = preview ? PowerShellFlags::Preview : static_cast<PowerShellFlags>(0);
                out.emplace_back(PowerShellInstance{ std::stoi(versionedPath.filename()),
                                                     flags,
                                                     executable });
            }
        }
    }
}

static winrt::Windows::ApplicationModel::Package _getStorePackage(const std::wstring_view packageFamilyName)
try
{
    winrt::Windows::Management::Deployment::PackageManager packageManager;
    auto foundPackages = packageManager.FindPackagesForUser(L"", packageFamilyName);
    auto iterator = foundPackages.First();
    if (!iterator.HasCurrent())
    {
        return nullptr;
    }
    return iterator.Current();
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return nullptr;
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
        auto previewPath = appExecAliasPath / POWERSHELL_PREVIEW_PFN;
        auto gaPath = appExecAliasPath / POWERSHELL_PFN;
        if (std::filesystem::exists(previewPath))
        {
            auto previewPackage = _getStorePackage(POWERSHELL_PREVIEW_PFN);
            if (previewPackage)
            {
                out.emplace_back(PowerShellInstance{
                    static_cast<int>(previewPackage.Id().Version().Major),
                    PowerShellFlags::Store | PowerShellFlags::Preview,
                    previewPath / PWSH_EXE });
            }
        }
        if (std::filesystem::exists(gaPath))
        {
            auto gaPackage = _getStorePackage(POWERSHELL_PFN);
            if (gaPackage)
            {
                out.emplace_back(PowerShellInstance{
                    gaPackage.Id().Version().Major,
                    PowerShellFlags::Store,
                    gaPath / PWSH_EXE,
                });
            }
        }
    }
}

static void _collectPwshExeInDirectory(const std::wstring_view directory, const PowerShellFlags flags, std::vector<PowerShellInstance>& out)
{
    std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(directory.data()) };
    auto pwshPath = root / PWSH_EXE;
    if (std::filesystem::exists(pwshPath))
    {
        out.emplace_back(PowerShellInstance{ 0, flags, pwshPath });
    }
}

static std::vector<PowerShellInstance> _enumeratePowerShellInstances()
{
    std::vector<PowerShellInstance> versions;
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles(x86)%\\PowerShell", versions);
    for (auto& v : versions)
    {
        // If they came from ProgramFiles(x86), they're not the native architecture.
        WI_SetFlag(v.flags, PowerShellFlags::WOW);
    }
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles%\\PowerShell", versions);
    _collectStorePowerShellInstances(versions);
    _collectPwshExeInDirectory(L"%USERPROFILE%\\.dotnet\\tools", PowerShellFlags::Dotnet, versions);
    _collectPwshExeInDirectory(L"%USERPROFILE%\\scoop\\shims", PowerShellFlags::Scoop, versions);
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

    auto psInstances = _enumeratePowerShellInstances();
    for (const auto& psI : psInstances)
    {
        std::wstring name = L"PowerShell";
        if (psI.majorVersion < 7)
        {
            name += L" Core";
        }
        if (psI.majorVersion != 0)
        {
            name += L" " + std::to_wstring(psI.majorVersion);
        }
        if (WI_IsFlagSet(psI.flags, PowerShellFlags::Preview))
        {
            name += L"-preview";
        }
        if (WI_IsFlagSet(psI.flags, PowerShellFlags::Store))
        {
            name += L" (Store)";
        }
        if (WI_IsFlagSet(psI.flags, PowerShellFlags::Dotnet))
        {
            name += L" (.NET Global)";
        }
        if (WI_IsFlagSet(psI.flags, PowerShellFlags::Scoop))
        {
            name += L" (Scoop)";
        }
        if (WI_IsFlagSet(psI.flags, PowerShellFlags::WOW))
        {
            name += L" (x86)";
        }

        auto profile{ CreateDefaultProfile(name) };
        profile.SetCommandline(psI.executablePath.wstring());
        profile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        profile.SetColorScheme({ L"Campbell" });

        profile.SetIconPath(WI_IsFlagSet(psI.flags, PowerShellFlags::Preview) ? POWERSHELL_PREVIEW_ICON : POWERSHELL_ICON);
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
