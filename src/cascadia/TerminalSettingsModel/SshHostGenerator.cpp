// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SshHostGenerator.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"

static constexpr std::wstring_view SshHostGeneratorNamespace{ L"Windows.Terminal.SSH" };

static constexpr std::wstring_view PROFILE_TITLE_PREFIX = L"SSH - ";
static constexpr std::wstring_view PROFILE_ICON_PATH = L"ms-appx:///ProfileIcons/{550ce7b8-d500-50ad-8a1a-c400c3262db3}.png";

// OpenSSH is installed under System32 when installed via Optional Features
static constexpr std::wstring_view SSH_EXE_PATH1 = L"%SystemRoot%\\System32\\OpenSSH\\ssh.exe";

// OpenSSH (x86/x64) is installed under Program Files when installed via MSI
static constexpr std::wstring_view SSH_EXE_PATH2 = L"%ProgramFiles%\\OpenSSH\\ssh.exe";

// OpenSSH (x86) is installed under Program Files x86 when installed via MSI on x64 machine
static constexpr std::wstring_view SSH_EXE_PATH3 = L"%ProgramFiles(x86)%\\OpenSSH\\ssh.exe";

static constexpr std::wstring_view SSH_SYSTEM_CONFIG_PATH = L"%ProgramData%\\ssh\\ssh_config";
static constexpr std::wstring_view SSH_USER_CONFIG_PATH = L"%UserProfile%\\.ssh\\config";

static constexpr std::wstring_view SSH_CONFIG_HOST_KEY{ L"Host" };
static constexpr std::wstring_view SSH_CONFIG_HOSTNAME_KEY{ L"HostName" };

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

/*static*/ const std::wregex SshHostGenerator::_configKeyValueRegex{ LR"(^\s*(\w+)\s+([^\s]+.*[^\s])\s*$)" };

/*static*/ std::wstring_view SshHostGenerator::_getProfileName(const std::wstring_view& hostName) noexcept
{
    return std::wstring_view{ L"" + PROFILE_TITLE_PREFIX + hostName };
}

/*static*/ std::wstring_view SshHostGenerator::_getProfileIconPath() noexcept
{
    return PROFILE_ICON_PATH;
}

/*static*/ std::wstring_view SshHostGenerator::_getProfileCommandLine(const std::wstring_view& sshExePath, const std::wstring_view& hostName) noexcept
{
    return std::wstring_view{ L"\"" + sshExePath + L"\" " + hostName };
}

/*static*/ bool SshHostGenerator::_tryFindSshExePath(std::wstring& sshExePath) noexcept
{
    try
    {
        for (const auto& path : { SSH_EXE_PATH1, SSH_EXE_PATH2, SSH_EXE_PATH3 })
        {
            if (std::filesystem::exists(wil::ExpandEnvironmentStringsW<std::wstring>(path.data())))
            {
                sshExePath = path;
                return true;
            }
        }
    }
    CATCH_LOG();

    return false;
}

/*static*/ bool SshHostGenerator::_tryParseConfigKeyValue(const std::wstring_view& line, std::wstring& key, std::wstring& value) noexcept
{
    try
    {
        if (!line.empty() && !line.starts_with(L"#"))
        {
            std::wstring input{ line };
            std::wsmatch match;
            if (std::regex_search(input, match, SshHostGenerator::_configKeyValueRegex))
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

/*static*/ void SshHostGenerator::_getHostNamesFromConfigFile(const std::wstring_view& configPath, std::vector<std::wstring>& hostNames) noexcept
{
    try
    {
        const std::filesystem::path resolvedConfigPath{ wil::ExpandEnvironmentStringsW<std::wstring>(configPath.data()) };
        if (std::filesystem::exists(resolvedConfigPath))
        {
            std::wifstream inputStream(resolvedConfigPath);

            std::wstring line;
            std::wstring key;
            std::wstring value;

            std::wstring lastHost;

            while (std::getline(inputStream, line))
            {
                if (_tryParseConfigKeyValue(line, key, value))
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
    if (_tryFindSshExePath(sshExePath))
    {
        std::vector<std::wstring> hostNames;

        _getHostNamesFromConfigFile(SSH_SYSTEM_CONFIG_PATH, hostNames);
        _getHostNamesFromConfigFile(SSH_USER_CONFIG_PATH, hostNames);

        for (const auto& hostName : hostNames)
        {
            const auto profile{ CreateDynamicProfile(_getProfileName(hostName)) };

            profile->Commandline(winrt::hstring{ _getProfileCommandLine(sshExePath, hostName) });
            profile->Icon(winrt::hstring{ _getProfileIconPath() });

            profiles.emplace_back(profile);
        }
    }
}
