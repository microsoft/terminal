// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "VsDevCmdGenerator.h"

using namespace Microsoft::Terminal::Settings::Model;

std::wstring VsDevCmdGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring name{ L"Developer Command Prompt for VS " };
    name.append(instance.GetProfileNameSuffix());
    return name;
}

std::wstring VsDevCmdGenerator::GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring commandLine{ L"cmd.exe /k \"" + instance.GetDevCmdScriptPath() + L"\"" };

    return commandLine;
}
