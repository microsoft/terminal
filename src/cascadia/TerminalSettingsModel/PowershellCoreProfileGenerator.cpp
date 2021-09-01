// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellCoreProfileGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

// These four are headers we do not want proliferating, so they're not in the PCH.
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
#include <appmodel.h>
#include <shlobj.h>

static constexpr std::wstring_view POWERSHELL_PFN{ L"Microsoft.PowerShell_8wekyb3d8bbwe" };
static constexpr std::wstring_view POWERSHELL_PREVIEW_PFN{ L"Microsoft.PowerShellPreview_8wekyb3d8bbwe" };
static constexpr std::wstring_view PWSH_EXE{ L"pwsh.exe" };
static constexpr std::wstring_view POWERSHELL_ICON{ L"ms-appx:///ProfileIcons/pwsh.png" };
static constexpr std::wstring_view POWERSHELL_PREVIEW_ICON{ L"ms-appx:///ProfileIcons/pwsh-preview.png" };
static constexpr std::wstring_view POWERSHELL_PREFERRED_PROFILE_NAME{ L"PowerShell" };

namespace
{
    enum PowerShellFlags
    {
        None = 0,

        // These flags are used as a sort key, so they encode some native ordering.
        // They are ordered such that the "most important" flags have the largest
        // impact on the sort space. For example, since we want Preview to be very polar
        // we give it the highest flag value.
        // The "ideal" powershell instance has 0 flags (stable, native, Program Files location)
        //
        // With this ordering, the sort space ends up being (for PowerShell 6)
        // (numerically greater values are on the left; this is flipped in the final sort)
        //
        // <-- Less Valued .................................... More Valued -->
        // |                 All instances of PS 6                 | All PS7  |
        // |          Preview          |          Stable           | ~~~      |
        // |  Non-Native | Native      |  Non-Native | Native      | ~~~      |
        // | Trd  | Pack | Trd  | Pack | Trd  | Pack | Trd  | Pack | ~~~      |
        // (where Pack is a stand-in for store, scoop, dotnet, though they have their own orders,
        // and Trd is a stand-in for "Traditional" (Program Files))
        //
        // In short, flags with larger magnitudes are pushed further down (therefore valued less)

        // distribution method (choose one)
        Store = 1 << 0, // distributed via the store
        Scoop = 1 << 1, // installed via Scoop
        Dotnet = 1 << 2, // installed as a dotnet global tool
        Traditional = 1 << 3, // installed in traditional Program Files locations

        // native architecture (choose one)
        WOWARM = 1 << 4, // non-native (Windows-on-Windows, ARM variety)
        WOWx86 = 1 << 5, // non-native (Windows-on-Windows, x86 variety)

        // build type (choose one)
        Preview = 1 << 6, // preview version
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

        // Method Description:
        // - Generates a name, based on flags, for a powershell instance.
        // Return value:
        // - the name
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
                namestream << L" (msix)";
            }
            else if (WI_IsFlagSet(flags, PowerShellFlags::Dotnet))
            {
                namestream << L" (dotnet global)";
            }
            else if (WI_IsFlagSet(flags, PowerShellFlags::Scoop))
            {
                namestream << L" (scoop)";
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

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

// Function Description:
// - Finds all powershell instances with the traditional layout under a directory.
// - The "traditional" directory layout requires that pwsh.exe exist in a versioned directory, as in
//   ROOT\6\pwsh.exe
// Arguments:
// - directory: the directory under which to search
// - flags: flags to apply to all found instances
// - out: the list into which to accumulate these instances.
static void _accumulateTraditionalLayoutPowerShellInstancesInDirectory(std::wstring_view directory, PowerShellFlags flags, std::vector<PowerShellInstance>& out)
{
    const std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(directory.data()) };
    if (std::filesystem::exists(root))
    {
        for (const auto& versionedDir : std::filesystem::directory_iterator(root))
        {
            const auto versionedPath = versionedDir.path();
            const auto executable = versionedPath / PWSH_EXE;
            if (std::filesystem::exists(executable))
            {
                const auto preview = versionedPath.filename().wstring().find(L"-preview") != std::wstring::npos;
                const auto previewFlag = preview ? PowerShellFlags::Preview : PowerShellFlags::None;
                out.emplace_back(PowerShellInstance{ std::stoi(versionedPath.filename()),
                                                     PowerShellFlags::Traditional | flags | previewFlag,
                                                     executable });
            }
        }
    }
}

// Function Description:
// - Finds the store package, if one exists, for a given package family name
// Arguments:
// - packageFamilyName: the package family name
// Return Value:
// - a package, or nullptr.
static winrt::Windows::ApplicationModel::Package _getStorePackage(const std::wstring_view packageFamilyName) noexcept
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

// Function Description:
// - Finds all powershell instances that have App Execution Aliases in the standard location
// Arguments:
// - out: the list into which to accumulate these instances.
static void _accumulateStorePowerShellInstances(std::vector<PowerShellInstance>& out)
{
    wil::unique_cotaskmem_string localAppDataFolder;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataFolder)))
    {
        return;
    }

    std::filesystem::path appExecAliasPath{ localAppDataFolder.get() };
    appExecAliasPath /= L"Microsoft";
    appExecAliasPath /= L"WindowsApps";

    if (std::filesystem::exists(appExecAliasPath))
    {
        // App execution aliases for preview powershell
        const auto previewPath = appExecAliasPath / POWERSHELL_PREVIEW_PFN;
        if (std::filesystem::exists(previewPath))
        {
            const auto previewPackage = _getStorePackage(POWERSHELL_PREVIEW_PFN);
            if (previewPackage)
            {
                out.emplace_back(PowerShellInstance{
                    gsl::narrow_cast<int>(previewPackage.Id().Version().Major),
                    PowerShellFlags::Store | PowerShellFlags::Preview,
                    previewPath / PWSH_EXE });
            }
        }

        // App execution aliases for stable powershell
        const auto gaPath = appExecAliasPath / POWERSHELL_PFN;
        if (std::filesystem::exists(gaPath))
        {
            const auto gaPackage = _getStorePackage(POWERSHELL_PFN);
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

// Function Description:
// - Finds a powershell instance that's just a pwsh.exe in a folder.
// - This function cannot determine the version number of such a powershell instance.
// Arguments:
// - directory: the directory under which to search
// - flags: flags to apply to all found instances
// - out: the list into which to accumulate these instances.
static void _accumulatePwshExeInDirectory(const std::wstring_view directory, const PowerShellFlags flags, std::vector<PowerShellInstance>& out)
{
    const std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(directory.data()) };
    const auto pwshPath = root / PWSH_EXE;
    if (std::filesystem::exists(pwshPath))
    {
        out.emplace_back(PowerShellInstance{ 0 /* we can't tell */, flags, pwshPath });
    }
}

// Function Description:
// - Builds a comprehensive priority-ordered list of powershell instances.
// Return value:
// - a comprehensive priority-ordered list of powershell instances.
static std::vector<PowerShellInstance> _collectPowerShellInstances()
{
    std::vector<PowerShellInstance> versions;

    _accumulateTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles%\\PowerShell", PowerShellFlags::None, versions);

#if defined(_M_AMD64) || defined(_M_ARM64) // No point in looking for WOW if we're not somewhere it exists
    _accumulateTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles(x86)%\\PowerShell", PowerShellFlags::WOWx86, versions);
#endif

#if defined(_M_ARM64) // no point in looking for WOA if we're not on ARM64
    _accumulateTraditionalLayoutPowerShellInstancesInDirectory(L"%ProgramFiles(Arm)%\\PowerShell", PowerShellFlags::WOWARM, versions);
#endif

    _accumulateStorePowerShellInstances(versions);

    _accumulatePwshExeInDirectory(L"%USERPROFILE%\\.dotnet\\tools", PowerShellFlags::Dotnet, versions);
    _accumulatePwshExeInDirectory(L"%USERPROFILE%\\scoop\\shims", PowerShellFlags::Scoop, versions);

    std::sort(versions.rbegin(), versions.rend()); // sort in reverse (best first)

    return versions;
}

// Legacy GUIDs:
//   - PowerShell Core       574e775e-4f2a-5b96-ac1e-a2962a402336
static constexpr winrt::guid PowershellCoreGuid{ 0x574e775e, 0x4f2a, 0x5b96, { 0xac, 0x1e, 0xa2, 0x96, 0x2a, 0x40, 0x23, 0x36 } };

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
std::vector<Profile> PowershellCoreProfileGenerator::GenerateProfiles()
{
    std::vector<Profile> profiles;

    auto psInstances = _collectPowerShellInstances();
    for (const auto& psI : psInstances)
    {
        const auto name = psI.Name();
        auto profile{ CreateDefaultProfile(name) };
        profile.Commandline(psI.executablePath.wstring());
        profile.StartingDirectory(DEFAULT_STARTING_DIRECTORY);
        profile.DefaultAppearance().ColorSchemeName(L"Campbell");

        profile.Icon(WI_IsFlagSet(psI.flags, PowerShellFlags::Preview) ? POWERSHELL_PREVIEW_ICON : POWERSHELL_ICON);
        profiles.emplace_back(std::move(profile));
    }

    if (profiles.size() > 0)
    {
        // Give the first ("algorithmically best") profile the official, and original, "PowerShell Core" GUID.
        // This will turn the anchored default profile into "PowerShell Core Latest for Native Architecture through Store"
        // (or the closest approximation thereof). It may choose a preview instance as the "best" if it is a higher version.
        auto firstProfile = profiles.begin();
        firstProfile->Guid(PowershellCoreGuid);
        firstProfile->Name(POWERSHELL_PREFERRED_PROFILE_NAME);
    }

    return profiles;
}

// Function Description:
// - Returns the thing it's named for.
// Return value:
// - the thing it says in the name
const std::wstring_view PowershellCoreProfileGenerator::GetPreferredPowershellProfileName()
{
    return POWERSHELL_PREFERRED_PROFILE_NAME;
}
