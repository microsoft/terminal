/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SshHostGenerator

Abstract:
- This is the dynamic profile generator for SSH connections. Enumerates all the
  SSH hosts to create profiles for them.

Author(s):
- Jon Thysell - September 2022

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class SshHostGenerator final : public IDynamicProfileGenerator
    {
    public:
        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

    private:
        static const std::wregex ConfigKeyValueRegex;

        static std::wstring GetProfileName(const std::wstring_view& hostName) noexcept;
        static std::wstring GetProfileIconPath() noexcept;
        static std::wstring GetProfileCommandLine(const std::wstring_view& sshExePath, const std::wstring_view& hostName) noexcept;

        static bool TryFindSshExePath(std::wstring& sshExePath) noexcept;
        static bool TryParseConfigKeyValue(const std::wstring_view& line, std::wstring& key, std::wstring& value) noexcept;
        static void GetHostNamesFromConfigFile(const std::wstring configPath, std::vector<std::wstring>& hostNames) noexcept;
    };
};
