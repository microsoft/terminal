// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
#include "VcDevCmdGenerator.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

void VcDevCmdGenerator::GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    try
    {
        const std::filesystem::path root{ GetVcCmdScriptDirectory(instance) };
        if (!std::filesystem::exists(root))
        {
            return;
        }

// x64 environments only installed on 64-bit machines.
#ifdef _WIN64
        const auto vcvars64 = root / L"vcvars64.bat";
        if (std::filesystem::exists(vcvars64))
        {
            auto profile = CreateProfile(instance, L"x64", vcvars64, hidden);
            profiles.emplace_back(std::move(profile));

            // Only the VC environment for the matching architecture should be shown by default.
            hidden = true;
        }

        const auto vcvarsamd64_x86 = root / L"vcvarsamd64_x86.bat";
        if (std::filesystem::exists(vcvarsamd64_x86))
        {
            auto profile = CreateProfile(instance, L"x64_x86", vcvarsamd64_x86, true);
            profiles.emplace_back(std::move(profile));
        }

        const auto vcvarsx86_amd64 = root / L"vcvarsx86_amd64.bat";
        if (std::filesystem::exists(vcvarsx86_amd64))
        {
            auto profile = CreateProfile(instance, L"x86_x64", vcvarsx86_amd64, true);
            profiles.emplace_back(std::move(profile));
        }
#endif // _WIN64

        const auto vcvars32 = root / L"vcvars32.bat";
        if (std::filesystem::exists(vcvars32))
        {
            auto profile = CreateProfile(instance, L"x86", vcvars32, hidden);
            profiles.emplace_back(std::move(profile));
        }
    }
    CATCH_LOG();
}

winrt::com_ptr<implementation::Profile> VcDevCmdGenerator::CreateProfile(const VsSetupConfiguration::VsSetupInstance& instance, const std::wstring_view& prefix, const std::filesystem::path& path, bool hidden) const
{
    const auto seed = GetProfileGuidSeed(instance, path);
    const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(seed))) };

    auto profile = winrt::make_self<implementation::Profile>(profileGuid);
    profile->Name(winrt::hstring{ GetProfileName(instance, prefix) });
    profile->Commandline(winrt::hstring{ GetProfileCommandLine(path) });
    profile->StartingDirectory(winrt::hstring{ instance.GetInstallationPath() });
    profile->Icon(winrt::hstring{ GetProfileIconPath() });
    profile->Hidden(hidden);

    return profile;
}

std::wstring VcDevCmdGenerator::GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance, const std::filesystem::path& path) const
{
    return L"VsDevCmd" + instance.GetInstanceId() + path.native();
}

std::wstring VcDevCmdGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance, const std::wstring_view& prefix) const
{
    std::wstring name{ prefix + L" Native Tools Command Prompt for VS " };
    name.append(instance.GetProfileNameSuffix());
    return name;
}

std::wstring VcDevCmdGenerator::GetProfileCommandLine(const std::filesystem::path& path) const
{
    std::wstring commandLine{ L"cmd.exe /k \"" + path.native() + L"\"" };

    return commandLine;
}

std::wstring VcDevCmdGenerator::GetVcCmdScriptDirectory(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    return instance.ResolvePath(L"VC\\Auxiliary\\Build\\");
}
