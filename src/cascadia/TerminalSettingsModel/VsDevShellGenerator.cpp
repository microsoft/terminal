// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
#include "VsDevShellGenerator.h"
#include "VsSetupConfiguration.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

void VsDevShellGenerator::GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    try
    {
        if (!IsInstanceValid(instance))
        {
            return;
        }

        const auto seed = GetProfileGuidSeed(instance);
        const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, std::as_bytes(std::span{ seed })) };
        auto profile = winrt::make_self<implementation::Profile>(profileGuid);
        profile->Name(winrt::hstring{ GetProfileName(instance) });
        profile->Commandline(winrt::hstring{ GetProfileCommandLine(instance) });
        profile->StartingDirectory(winrt::hstring{ instance.GetInstallationPath() });
        profile->Icon(winrt::hstring{ GetProfileIconPath() });
        profile->Hidden(hidden);
        profiles.emplace_back(std::move(profile));
    }
    CATCH_LOG();
}

std::wstring VsDevShellGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring name{ L"Developer PowerShell for VS " };
    name.append(instance.GetProfileNameSuffix());
    return name;
}

std::wstring VsDevShellGenerator::GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    // Build this in stages, so reserve space now
    std::wstring commandLine;
    commandLine.reserve(256);

    // Try to detect if `pwsh.exe` is available in the PATH, if so we want to use that
    // Allow some extra space in case user put it somewhere odd
    // We do need to allocate space for the full path even if we don't want to paste the whole thing in
    wchar_t pwshPath[MAX_PATH] = { 0 };
    const auto pwshExeName = L"pwsh.exe";
    if (SearchPathW(nullptr, pwshExeName, nullptr, MAX_PATH, pwshPath, nullptr))
    {
        commandLine.append(pwshExeName);
    }
    else
    {
        commandLine.append(L"powershell.exe");
    }

    // The triple-quotes are a PowerShell path escape sequence that can safely be stored in a JSON object.
    // The "SkipAutomaticLocation" parameter will prevent "Enter-VsDevShell" from automatically setting the shell path
    // so the path in the profile will be used instead
    commandLine.append(LR"( -NoExit -Command "&{Import-Module """)");
    commandLine.append(GetDevShellModulePath(instance));
    commandLine.append(LR"("""; Enter-VsDevShell )");
    commandLine.append(instance.GetInstanceId());
#if defined(_M_ARM64)
    // This part stays constant no matter what
    commandLine.append(LR"( -SkipAutomaticLocation -DevCmdArguments """-arch=arm64 -host_arch=)");
    if (instance.VersionInRange(L"[17.4,"))
    {
        commandLine.append(LR"("arm64 """}")");
    }
    // If an old version of VS is installed without ARM64 host support
    else
    {
        commandLine.append(LR"("x64 """}")");
    }
#elif defined(_M_AMD64)
    commandLine.append(LR"( -SkipAutomaticLocation -DevCmdArguments """-arch=x64 -host_arch=x64"""}")");
#else
    commandLine.append(LR"( -SkipAutomaticLocation}")");
#endif

    return commandLine;
}

std::wstring VsDevShellGenerator::GetDevShellModulePath(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    // The path of Microsoft.VisualStudio.DevShell.dll changed in 16.3
    if (instance.VersionInRange(L"[16.3,"))
    {
        return instance.ResolvePath(L"Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll");
    }

    return instance.ResolvePath(L"Common7\\Tools\\vsdevshell\\Microsoft.VisualStudio.DevShell.dll");
}
