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
    std::wstring_view GetNamespace() override;

    std::vector<TerminalApp::Profile> GenerateProfiles() override;

private:
    static bool _isPowerShellCoreInstalled(std::filesystem::path& cmdline);
    static bool _isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline);
};
