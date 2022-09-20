// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SshHostGenerator.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"

static constexpr std::wstring_view SshHostGeneratorNamespace{ L"Windows.Terminal.SSH" };

static constexpr const wchar_t* PROFILE_TITLE_PREFIX = L"SSH - ";

static constexpr const wchar_t* SSH_EXE_PATH1 = L"%SystemRoot%\\System32\\OpenSSH\\ssh.exe";
static constexpr const wchar_t* SSH_EXE_PATH2 = L"%ProgramFiles%\\OpenSSH\\ssh.exe";
static constexpr const wchar_t* SSH_EXE_PATH3 = L"%ProgramFiles(x86)%\\OpenSSH\\ssh.exe";

static constexpr const wchar_t* SSH_SYSTEMCONFIG_PATH = L"%ProgramData%\\ssh\\ssh_config";
static constexpr const wchar_t* SSH_USERCONFIG_PATH = L"%UserProfile%\\.ssh\\config";

static constexpr std::wstring_view SSH_HOST_PREFIX{ L"Host " };

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

std::wstring_view SshHostGenerator::GetNamespace() const noexcept
{
    return SshHostGeneratorNamespace;
}

static bool tryFindSshExePath(std::wstring& sshExePath) noexcept
{
    try
    {
        // OpenSSH is installed under System32 when installed via Optional Features
        if (std::filesystem::exists(wil::ExpandEnvironmentStringsW<std::wstring>(SSH_EXE_PATH1)))
        {
            sshExePath = SSH_EXE_PATH1;
            return true;
        }

        // OpenSSH (x86/x64) is installed under Program Files when installed via MSI
        if (std::filesystem::exists(wil::ExpandEnvironmentStringsW<std::wstring>(SSH_EXE_PATH2)))
        {
            sshExePath = SSH_EXE_PATH2;
            return true;
        }

        // OpenSSH (x86) is installed under Program Files x86 when installed via MSI on x64 machine
        if (std::filesystem::exists(wil::ExpandEnvironmentStringsW<std::wstring>(SSH_EXE_PATH3)))
        {
            sshExePath = SSH_EXE_PATH3;
            return true;
        }
    }
    CATCH_LOG();

    return false;
}

static winrt::com_ptr<implementation::Profile> makeProfile(const std::wstring& sshExePath, const std::wstring& hostName)
{
    const auto profile{ CreateDynamicProfile(PROFILE_TITLE_PREFIX + hostName) };

    profile->Commandline(winrt::hstring{ L"\"" + sshExePath + L"\" " + hostName });

    return profile;
}

static void tryGetHostNamesFromConfigFile(const std::wstring configPath, std::vector<std::wstring>& hostNames) noexcept
{
    try
    {
        const std::filesystem::path resolvedConfigPath{ wil::ExpandEnvironmentStringsW<std::wstring>(configPath.c_str()) };
        if (std::filesystem::exists(resolvedConfigPath))
        {
            std::wifstream inputStream(resolvedConfigPath);

            std::wstring line;
            while (std::getline(inputStream, line))
            {
                if (line.starts_with(SSH_HOST_PREFIX))
                {
                    hostNames.emplace_back(line.substr(SSH_HOST_PREFIX.length()));
                }
            }
        }
    }
    CATCH_LOG();
}

// Method Description:
// - Generate a list of profiles for each detected OpenSSH host.
// Arguments:
// - <none>
// Return Value:
// - <A list of SSH host profiles.>
void SshHostGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    std::wstring sshExePath;
    if (tryFindSshExePath(sshExePath))
    {
        std::vector<std::wstring> hostNames;

        tryGetHostNamesFromConfigFile(SSH_SYSTEMCONFIG_PATH, hostNames);
        tryGetHostNamesFromConfigFile(SSH_USERCONFIG_PATH, hostNames);

        for (const auto& hostName : hostNames)
        {
            profiles.emplace_back(makeProfile(sshExePath, hostName));
        }
    }
}
