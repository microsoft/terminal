// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseVisualStudioGenerator.h"
#include "DefaultProfileUtils.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

void BaseVisualStudioGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
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

            const auto seed = GetProfileGuidSeed(instance);
            const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(seed))) };
            auto profile = winrt::make_self<implementation::Profile>(profileGuid);
            profile->Origin(OriginTag::Generated);
            profile->Name(winrt::hstring{ GetProfileName(instance) });
            profile->Commandline(winrt::hstring{ GetProfileCommandLine(instance) });
            profile->StartingDirectory(winrt::hstring{ instance.GetInstallationPath() });
            profile->Icon(winrt::hstring{ GetProfileIconPath() });
            profiles.emplace_back(std::move(profile));
        }
        CATCH_LOG();
    }
}
