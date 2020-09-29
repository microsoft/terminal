// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "VsDevCmdGenerator.h"

std::wstring TerminalApp::VsDevCmdGenerator::GetProfileName(const VsSetupConfiguration::VsSetupInstance instance) const
{
    std::wstring name{ L"Developer Command Prompt for VS " };
    name.append(instance.GetProductLineVersion());

    std::wstring channelName{ instance.GetChannelName() };

    if (channelName != L"Release")
    {
        name.append(L" [" + channelName + L"]");
    }

    return name;
}

std::wstring TerminalApp::VsDevCmdGenerator::GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance instance) const
{
    std::wstring commandLine{ L"cmd.exe /k \"" + instance.GetDevCmdScriptPath() + L"\"" };

    OutputDebugString(commandLine.c_str());
    OutputDebugString(L"\n");

    return commandLine;
}
