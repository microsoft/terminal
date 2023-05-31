// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
#include "VsDevCmdGenerator.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

void VsDevCmdGenerator::GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
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

std::wstring VsDevCmdGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring name{ L"Developer Command Prompt for VS " };
    name.append(instance.GetProfileNameSuffix());
    return name;
}

std::wstring VsDevCmdGenerator::GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring commandLine;
    commandLine.reserve(256);
    commandLine.append(LR"(cmd.exe /k ")");
    commandLine.append(GetDevCmdScriptPath(instance));
    // The "-startdir" parameter will prevent "vsdevcmd" from automatically
    // setting the shell path so the path in the profile will be used instead.
#if defined(_M_ARM64)
    commandLine.append(LR"(" -startdir=none -arch=arm64 -host_arch=x64)");
#elif defined(_M_AMD64)
    commandLine.append(LR"(" -startdir=none -arch=x64 -host_arch=x64)");
#else
    commandLine.append(LR"(" -startdir=none)");
#endif
    return commandLine;
}

std::wstring VsDevCmdGenerator::GetDevCmdScriptPath(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    return instance.ResolvePath(L"Common7\\Tools\\VsDevCmd.bat");
}
