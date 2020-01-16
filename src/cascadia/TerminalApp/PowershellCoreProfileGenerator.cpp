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
        // These flags are used as a sort key, so they encode some native ordering
        // They are ordered such that the "most important" flags have the largest
        // impact on the sort space. For example, since we want Preview to be very polar
        // we give it the highest flag value.
        // The "ideal" powershell instance has 0 flags (stable, native, Program Files location)
        //
        // With this ordering, the sort space ends up being (for PowerShell 6)
        // (numerically greater values are on the left; this is flipped in the final sort)
        // <-- Less Valued                                      More Valued -->
        // |                 All instances of PS 6                 | All PS7  |
        // |          Preview          |          Stable           | ~~~      |
        // |  Non-Native | Native      |  Non-Native | Native      | ~~~      |
        // | Pack | Lega | Pack | Lega | Pack | Lega | Pack | Lega | ~~~      |
        // (where Pack is a stand-in for store, scoop, dotnet, though they have their own orders)
        // From this you can determine:
        // All legacy-installed (program files) native preview versions are _always_
        // less important than any non-preview (GA) versions.

        // distribution method (choose one)
        Store = 1 << 0, // distributed via the store
        Scoop = 1 << 1, // installed via Scoop
        Dotnet = 1 << 2, // installed as a dotnet global tool

        // native architecutre (choose one)
        WOWx86 = 1 << 3, // non-native (Windows-on-Windows, x86 variety)
        WOWARM = 1 << 4, // non-native (Windows-on-Windows, ARM variety)

        // build type (choose one)
        Preview = 1 << 5, // preview version
    };
    DEFINE_ENUM_FLAG_OPERATORS(PowerShellFlags);

    struct PowerShellInstance
    {
        int majorVersion; // 0 = we don't know, sort last.
        PowerShellFlags flags;
        std::filesystem::path executablePath;

        constexpr bool operator<(const PowerShellInstance& second) const
        {
            if (majorVersion != second.majorVersion)
            {
                return majorVersion < second.majorVersion;
            }
            if (flags != second.flags)
            {
                return flags > second.flags; // flags are inverted because "0" is ideal; see above
            }
            return executablePath < second.executablePath; // fall back to path sorting
        }

        std::wstring Name() const
        {
            std::wstringstream namestream;
            namestream << L"PowerShell";

            if (WI_IsFlagSet(flags, PowerShellFlags::Store))
            {
                if (WI_IsFlagSet(flags, PowerShellFlags::Preview))
                {
                    namestream << L" Preview";
                }
                namestream << L" (MSIX)";
            }
            else if (WI_IsFlagSet(flags, PowerShellFlags::Dotnet))
            {
                namestream << L" (.NET Global)";
            }
            else if (WI_IsFlagSet(flags, PowerShellFlags::Scoop))
            {
                namestream << L" (Scoop)";
            }
            else
            {
                if (majorVersion < 7)
                {
                    namestream << L" Core";
                }
                if (majorVersion != 0)
                {
                    namestream << L" " << majorVersion;
                }
                if (WI_IsFlagSet(flags, PowerShellFlags::Preview))
                {
                    namestream << L" Preview";
                }
                if (WI_IsFlagSet(flags, PowerShellFlags::WOWx86))
                {
                    namestream << L" (x86)";
                }
                if (WI_IsFlagSet(flags, PowerShellFlags::WOWARM))
                {
                    namestream << L" (ARM)";
                }
            }
            return namestream.str();
        }
    };
}

using namespace ::TerminalApp;

static void _collectTraditionalLayoutPowerShellInstancesInDirectory(std::wstring_view directory, std::vector<PowerShellInstance>& out)
{
    std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(directory.data()) };
    if (std::filesystem::exists(root))
    {
        for (const auto& versionedDir : std::filesystem::directory_iterator(root))
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
        // App execution aliases for preview powershell
        auto previewPath = appExecAliasPath / POWERSHELL_PREVIEW_PFN;
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

        // App execution aliases for stable powershell
        auto gaPath = appExecAliasPath / POWERSHELL_PFN;
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

static void _promotePwshFromPath(std::vector<PowerShellInstance>& out)
{
    (void)out;
}

static std::vector<PowerShellInstance> _enumeratePowerShellInstances()
{
    std::vector<PowerShellInstance> versions;
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles(x86)%\\PowerShell", versions);
    for (auto& v : versions)
    {
        // If they came from ProgramFiles(x86), they're not the native architecture.
        WI_SetFlag(v.flags, PowerShellFlags::WOWx86);
    }
    _collectTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles%\\PowerShell", versions);
    _collectStorePowerShellInstances(versions);
    _collectPwshExeInDirectory(L"%USERPROFILE%\\.dotnet\\tools", PowerShellFlags::Dotnet, versions);
    _collectPwshExeInDirectory(L"%USERPROFILE%\\scoop\\shims", PowerShellFlags::Scoop, versions);

    std::sort(versions.rbegin(), versions.rend()); // sort in reverse (best first)

    // Now that we're sorted, promote the one found first in PATH (as the user might want that one by default)
    _promotePwshFromPath(versions);

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
        auto name = psI.Name();
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
