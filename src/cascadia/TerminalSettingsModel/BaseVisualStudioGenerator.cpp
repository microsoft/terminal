// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseVisualStudioGenerator.h"
#include "DefaultProfileUtils.h"

using namespace Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model;

std::vector<Profile>
    Model::BaseVisualStudioGenerator::GenerateProfiles()
{
    std::vector<Profile> profiles;

    // There's no point in enumerating valid Visual Studio instances more than once,
    // so cache them for use by both Visual Studio profile generators.
    if (!BaseVisualStudioGenerator::hasQueried)
    {
        instances = VsSetupConfiguration::QueryInstances();
        hasQueried = true;
    }

    for (auto const& instance : BaseVisualStudioGenerator::instances)
    {
        try
        {
            if (!IsInstanceValid(instance))
                continue;

            auto DevShell{ CreateDefaultProfile(GetProfileName(instance)) };
            DevShell.Commandline(GetProfileCommandLine(instance));
            DevShell.StartingDirectory(instance.GetInstallationPath());
            DevShell.Icon(GetProfileIconPath());

            profiles.emplace_back(DevShell);
        }
        CATCH_LOG();
    }

    return profiles;
}
