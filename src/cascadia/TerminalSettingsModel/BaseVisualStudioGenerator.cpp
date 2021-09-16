// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseVisualStudioGenerator.h"
#include "DefaultProfileUtils.h"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

std::vector<Profile> BaseVisualStudioGenerator::GenerateProfiles()
{
    std::vector<Profile> profiles;

    // There's no point in enumerating valid Visual Studio instances more than once,
    // so cache them for use by both Visual Studio profile generators.
    static const auto instances = VsSetupConfiguration::QueryInstances();

    for (auto const& instance : instances)
    {
        try
        {
            if (!IsInstanceValid(instance))
            {
                continue;
            }

            auto DevShell{ CreateProfile(GetProfileGuidSeed(instance)) };
            DevShell.Name(GetProfileName(instance));
            DevShell.Commandline(GetProfileCommandLine(instance));
            DevShell.StartingDirectory(instance.GetInstallationPath());
            DevShell.Icon(GetProfileIconPath());

            profiles.emplace_back(DevShell);
        }
        CATCH_LOG();
    }

    return profiles;
}

Profile BaseVisualStudioGenerator::CreateProfile(const std::wstring_view seed)
{
    const winrt::guid profileGuid{ Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID,
                                                                           gsl::as_bytes(gsl::make_span(seed))) };

    auto newProfile = winrt::make_self<implementation::Profile>(profileGuid);
    newProfile->Origin(OriginTag::Generated);

    return *newProfile;
}
