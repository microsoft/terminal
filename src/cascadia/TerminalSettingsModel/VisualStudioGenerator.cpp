// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
#include "VisualStudioGenerator.h"
#include "VsDevCmdGenerator.h"
#include "VsDevShellGenerator.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

std::wstring_view VisualStudioGenerator::GetNamespace() const noexcept
{
    return std::wstring_view{ L"Windows.Terminal.VisualStudio" };
}

void VisualStudioGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    const auto instances = VsSetupConfiguration::QueryInstances();

    VsDevCmdGenerator devCmdGenerator;
    VsDevShellGenerator devShellGenerator;

    // Instances are ordered from latest to oldest. Hide all but the profiles for the latest instance.
    auto hidden = false;
    for (const auto& instance : instances)
    {
        devCmdGenerator.GenerateProfiles(instance, hidden, profiles);
        devShellGenerator.GenerateProfiles(instance, hidden, profiles);
        hidden = true;
    }
}
