// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SshHostGenerator.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"

static constexpr std::wstring_view SshHostGeneratorNamespace{ L"Windows.Terminal.SSH" };

static constexpr std::wstring_view PROFILE_TITLE_PREFIX = L"SSH - ";
static constexpr std::wstring_view PROFILE_ICON_PATH = L"\uE977"; // PC1
static constexpr std::wstring_view GENERATOR_ICON_PATH = L"\uE969"; // StorageNetworkWireless

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
static constexpr std::wstring_view SSH_CONFIG_INCLUDE_KEY{ L"Include" };

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

/*static*/ const std::wregex SshHostGenerator::_configKeyValueRegex{ LR"(^\s*(\w+)\s+([^\s]+.*[^\s])\s*$)" };

winrt::hstring _getProfileName(const std::wstring_view& hostName) noexcept
{
    return til::hstring_format(FMT_COMPILE(L"{0}{1}"), PROFILE_TITLE_PREFIX, hostName);
}

winrt::hstring _getProfileCommandLine(const std::wstring_view& sshExePath, const std::wstring_view& hostName) noexcept
{
    return til::hstring_format(FMT_COMPILE(LR"("{0}" {1})"), sshExePath, hostName);
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
                    else if (til::equals_insensitive_ascii(key, SSH_CONFIG_INCLUDE_KEY))
                    {
                        // Handle Include directive
                        _processIncludeDirective(resolvedConfigPath.parent_path(), value, hostNames);
                    }
                }
            }
        }
    }
    CATCH_LOG();
}

/*static*/ void SshHostGenerator::_processIncludeDirective(const std::filesystem::path& configDir, const std::wstring_view& includePattern, std::vector<std::wstring>& hostNames) noexcept
{
    try
    {
        // Resolve the include pattern relative to config directory
        std::filesystem::path includePath = configDir / std::wstring(includePattern);
        
        // Handle wildcards by checking parent directory
        if (includePattern.find(L'*') != std::wstring_view::npos || includePattern.find(L'?') != std::wstring_view::npos)
        {
            const auto parentPath = includePath.parent_path();
            const auto pattern = includePath.filename().wstring();
            
            if (std::filesystem::exists(parentPath) && std::filesystem::is_directory(parentPath))
            {
                for (const auto& entry : std::filesystem::directory_iterator(parentPath))
                {
                    if (entry.is_regular_file())
                    {
                        const auto filename = entry.path().filename().wstring();
                        // Simple wildcard matching: * matches any characters
                        if (_matchesPattern(filename, pattern))
                        {
                            _getHostNamesFromConfigFile(entry.path().wstring(), hostNames);
                        }
                    }
                }
            }
        }
        else
        {
            // Direct file include (no wildcards)
            if (std::filesystem::exists(includePath))
            {
                _getHostNamesFromConfigFile(includePath.wstring(), hostNames);
            }
        }
    }
    CATCH_LOG();
}

/*static*/ bool SshHostGenerator::_matchesPattern(const std::wstring_view& filename, const std::wstring_view& pattern) noexcept
{
    try
    {
        // Simple wildcard matching: * matches any characters, ? matches single character
        size_t filenamePos = 0;
        size_t patternPos = 0;
        
        while (filenamePos < filename.length() && patternPos < pattern.length())
        {
            if (pattern[patternPos] == L'*')
            {
                // Skip consecutive asterisks
                while (patternPos < pattern.length() && pattern[patternPos] == L'*')
                {
                    patternPos++;
                }
                
                // If * is at the end, match everything
                if (patternPos == pattern.length())
                {
                    return true;
                }
                
                // Try to match the rest of the pattern
                while (filenamePos < filename.length())
                {
                    if (_matchesPattern(filename.substr(filenamePos), pattern.substr(patternPos)))
                    {
                        return true;
                    }
                    filenamePos++;
                }
                return false;
            }
            else if (pattern[patternPos] == L'?' || pattern[patternPos] == filename[filenamePos])
            {
                filenamePos++;
                patternPos++;
            }
            else
            {
                return false;
            }
        }
        
        // Handle remaining asterisks in pattern
        while (patternPos < pattern.length() && pattern[patternPos] == L'*')
        {
            patternPos++;
        }
        
        return filenamePos == filename.length() && patternPos == pattern.length();
    }
    CATCH_LOG();
    return false;
}

std::wstring_view SshHostGenerator::GetNamespace() const noexcept
{
    return SshHostGeneratorNamespace;
}

std::wstring_view SshHostGenerator::GetDisplayName() const noexcept
{
    return RS_(L"SshHostGeneratorDisplayName");
}

std::wstring_view SshHostGenerator::GetIcon() const noexcept
{
    return GENERATOR_ICON_PATH;
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

            profile->Commandline(_getProfileCommandLine(sshExePath, hostName));
            profile->Icon(winrt::hstring{ PROFILE_ICON_PATH });

            profiles.emplace_back(profile);
        }
    }
}
