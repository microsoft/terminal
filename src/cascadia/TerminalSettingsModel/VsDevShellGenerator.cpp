// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "VsDevShellGenerator.h"
#include "VsSetupConfiguration.h"

using namespace Microsoft::Terminal::Settings::Model;

std::wstring VsDevShellGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    std::wstring name{ L"Developer PowerShell for VS " };
    name.append(instance.GetProfileNameSuffix());
    return name;
}

std::wstring VsDevShellGenerator::GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const
{
    // The triple-quotes are a PowerShell path escape sequence that can safely be stored in a JSON object.
    // The "SkipAutomaticLocation" parameter will prevent "Enter-VsDevShell" from automatically setting the shell path
    // so the path in the profile will be used instead.
    std::wstring commandLine{ L"powershell.exe -NoExit -Command \"& {" };
    commandLine.append(L"Import-Module \"\"\"" + instance.GetDevShellModulePath() + L"\"\"\";");
    commandLine.append(L"Enter-VsDevShell " + instance.GetInstanceId() + L" -SkipAutomaticLocation");
    commandLine.append(L"}\"");

    return commandLine;
}
