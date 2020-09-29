// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseVisualStudioGenerator.h"
#include "DefaultProfileUtils.h"

std::vector<winrt::TerminalApp::Profile> TerminalApp::BaseVisualStudioGenerator::GenerateProfiles()
{
    std::vector<winrt::TerminalApp::Profile> profiles;

    for (auto const& instance : VsSetupConfiguration::QueryInstances())
    {
        try
        {
            instance.DebugOutputProperties();
            if (!IsInstanceValid(instance))
                continue;

            auto DevShell{ CreateDefaultProfile(GetProfileName(instance)) };
            DevShell.Commandline(GetProfileCommandLine(instance));
            DevShell.StartingDirectory(instance.GetInstallationPath());
            DevShell.IconPath(GetProfileIconPath());

            profiles.emplace_back(DevShell);
        }
        catch (const wil::ResultException&)
        {
            continue;
        }
    }

    return profiles;
}
