// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "PowershellInstallationProfileGenerator.h"
#include "DynamicProfileUtils.h"

#include <LibraryResources.h>

static constexpr std::wstring_view POWERSHELL_ICON{ L"ms-appx:///ProfileIcons/pwsh.png" };
static constexpr std::wstring_view POWERSHELL_ICON_64{ L"ms-appx:///ProfileIcons/Powershell_black_64.png" };

namespace winrt::Microsoft::Terminal::Settings::Model
{
    std::wstring_view PowershellInstallationProfileGenerator::Namespace{ L"Windows.Terminal.InstallPowerShell" };

    std::wstring_view PowershellInstallationProfileGenerator::GetNamespace() const noexcept
    {
        return Namespace;
    }

    std::wstring_view PowershellInstallationProfileGenerator::GetDisplayName() const noexcept
    {
        return RS_(L"PowerShellInstallationProfileGeneratorDisplayName");
    }

    std::wstring_view PowershellInstallationProfileGenerator::GetIcon() const noexcept
    {
        return POWERSHELL_ICON_64;
    }

    void PowershellInstallationProfileGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles)
    {
        auto profile{ CreateDynamicProfile(RS_(L"PowerShellInstallationProfileName")) };
        profile->Commandline(winrt::hstring{ fmt::format(FMT_COMPILE(L"cmd /k winget install --interactive --id Microsoft.PowerShell & echo. & echo {} & exit"), RS_(L"PowerShellInstallationInstallerGuidance")) });
        profile->Icon(winrt::hstring{ POWERSHELL_ICON });
        profile->CloseOnExit(CloseOnExitMode::Never);

        profiles.emplace_back(std::move(profile));
    }
}
