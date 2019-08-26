/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
-

Abstract:
-
Author(s):
- Mike Griese - August 2019

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace TerminalApp
{
    class PowershellCoreProfileGenerator;
};

class TerminalApp::PowershellCoreProfileGenerator : public TerminalApp::IDynamicProfileGenerator
{
public:
    PowershellCoreProfileGenerator() = default;
    ~PowershellCoreProfileGenerator() = default;
    std::wstring GetNamespace() override;

    std::vector<TerminalApp::Profile> GenerateProfiles() override;

private:
    static constexpr std::wstring_view LegacyGuid{ L"{574e775e-4f2a-5b96-ac1e-a2962a402336}" };

    static bool _isPowerShellCoreInstalled(std::filesystem::path& cmdline);
    static bool _isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline);
};
