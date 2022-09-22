// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SshHostGenerator.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"

static constexpr std::wstring_view SshHostGeneratorNamespace{ L"Windows.Terminal.SSH" };

static constexpr const wchar_t* PROFILE_TITLE_PREFIX = L"SSH - ";
static constexpr const wchar_t* PROFILE_ICON_PATH = L"ms-appx:///ProfileIcons/{550ce7b8-d500-50ad-8a1a-c400c3262db3}.png";

static constexpr const wchar_t* SSH_EXE_PATH1 = L"%SystemRoot%\\System32\\OpenSSH\\ssh.exe";
static constexpr const wchar_t* SSH_EXE_PATH2 = L"%ProgramFiles%\\OpenSSH\\ssh.exe";
static constexpr const wchar_t* SSH_EXE_PATH3 = L"%ProgramFiles(x86)%\\OpenSSH\\ssh.exe";

static constexpr const wchar_t* SSH_SYSTEMCONFIG_PATH = L"%ProgramData%\\ssh\\ssh_config";
static constexpr const wchar_t* SSH_USERCONFIG_PATH = L"%UserProfile%\\.ssh\\config";

static constexpr std::wstring_view SSH_CONFIG_HOST_KEY{ L"Host" };
static constexpr std::wstring_view SSH_CONFIG_HOSTNAME_KEY{ L"HostName" };

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

/*static*/ const std::wregex SshHostGenerator::ConfigKeyValueRegex{ LR"(^\s*(\w+)\s+([^\s]+.*[^\s])\s*$)" };

/*static*/ std::wstring SshHostGenerator::GetProfileName(const std::wstring_view& hostName) noexcept
{
    return std::wstring{ PROFILE_TITLE_PREFIX + hostName };
}

/*static*/ std::wstring SshHostGenerator::GetProfileIconPath() noexcept
{
    return PROFILE_ICON_PATH;
}

/*static*/ std::wstring SshHostGenerator::GetProfileCommandLine(const std::wstring_view& sshExePath, const std::wstring_view& hostName) noexcept
{
    return std::wstring(L"\"" + sshExePath + L"\" " + hostName);
}

/*static*/ bool SshHostGenerator::TryFindSshExePath(std::wstring& sshExePath) noexcept
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

/*static*/ bool SshHostGenerator::TryParseConfigKeyValue(const std::wstring_view& line, std::wstring& key, std::wstring& value) noexcept
{
    try
    {
        if (!line.empty() && !line.starts_with(L"#"))
        {
            std::wstring input{ line };
            std::wsmatch match;
            if (std::regex_search(input, match, SshHostGenerator::ConfigKeyValueRegex))
            {
                key = match[1];
                value = match[2];
                return true;
            }
        }
    }
    CATCH_LOG();

    return false;
}

/*static*/ void SshHostGenerator::GetHostNamesFromConfigFile(const std::wstring configPath, std::vector<std::wstring>& hostNames) noexcept
{
    try
    {
        const std::filesystem::path resolvedConfigPath{ wil::ExpandEnvironmentStringsW<std::wstring>(configPath.c_str()) };
        if (std::filesystem::exists(resolvedConfigPath))
        {
            std::wifstream inputStream(resolvedConfigPath);

            std::wstring line;
            std::wstring key;
            std::wstring value;

            std::wstring lastHost;

            while (std::getline(inputStream, line))
            {
                if (TryParseConfigKeyValue(line, key, value))
                {
                    if (til::equals_insensitive_ascii(key, SSH_CONFIG_HOST_KEY))
                    {
                        // Save potential Host value for later
                        lastHost = value;
                    }
                    else if (til::equals_insensitive_ascii(key, SSH_CONFIG_HOSTNAME_KEY))
                    {
                        // HostName was specified
                        if (!lastHost.empty())
                        {
                            hostNames.emplace_back(lastHost);
                            lastHost = L"";
                        }
                    }
                }
            }
        }
    }
    CATCH_LOG();
}

std::wstring_view SshHostGenerator::GetNamespace() const noexcept
{
    return SshHostGeneratorNamespace;
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
    if (TryFindSshExePath(sshExePath))
    {
        std::vector<std::wstring> hostNames;

        GetHostNamesFromConfigFile(SSH_SYSTEMCONFIG_PATH, hostNames);
        GetHostNamesFromConfigFile(SSH_USERCONFIG_PATH, hostNames);

        for (const auto& hostName : hostNames)
        {
            const auto profile{ CreateDynamicProfile(GetProfileName(hostName)) };

            profile->Commandline(winrt::hstring{ GetProfileCommandLine(sshExePath, hostName) });
            profile->Icon(winrt::hstring{ GetProfileIconPath() });

            profiles.emplace_back(profile);
        }
    }
}
