// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseVisualStudioGenerator.h"
#include "DynamicProfileUtils.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

void BaseVisualStudioGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    // There's no point in enumerating valid Visual Studio instances more than once,
    // so cache them for use by both Visual Studio profile generators.
    static auto instances = VsSetupConfiguration::QueryInstances();

    // Sort instances based on version and install date.
    std::sort(instances.begin(), instances.end(), BaseVisualStudioGenerator::Compare);

    // Iterate through instances from newest to oldest.
    bool latest = true;
    for (auto it = instances.rbegin(); it != instances.rend(); it++)
    {
        auto const& instance = *it;

        try
        {
            if (!IsInstanceValid(instance))
            {
                continue;
            }

            const auto seed = GetProfileGuidSeed(instance);
            const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(seed))) };
            auto profile = winrt::make_self<implementation::Profile>(profileGuid);
            profile->Name(winrt::hstring{ GetProfileName(instance) });
            profile->Commandline(winrt::hstring{ GetProfileCommandLine(instance) });
            profile->StartingDirectory(winrt::hstring{ instance.GetInstallationPath() });
            profile->Icon(winrt::hstring{ GetProfileIconPath() });
            profile->Hidden(!latest);
            profiles.emplace_back(std::move(profile));

            latest = false;
        }
        CATCH_LOG();
    }
}

bool BaseVisualStudioGenerator::Compare(const VsSetupConfiguration::VsSetupInstance& a, const VsSetupConfiguration::VsSetupInstance& b)
{
    auto const aVersion = a.GetComparableVersion();
    auto const bVersion = b.GetComparableVersion();

    if (aVersion == bVersion)
    {
        return a.GetComparableInstallDate() < b.GetComparableInstallDate();
    }

    return aVersion < bVersion;
}
