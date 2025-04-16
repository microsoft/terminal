// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
#include "VisualStudioGenerator.h"
#include "VsDevCmdGenerator.h"
#include "VsDevShellGenerator.h"
#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings::Model;

std::wstring_view VisualStudioGenerator::Namespace{ L"Windows.Terminal.VisualStudio" };
// TODO CARLOS: Can we get the plain Visual Studio logo that looks like this: |><| ?
static constexpr std::wstring_view IconHighRes{ L"ms-appx:///ProfileIcons/vs-pwsh.scale-200.png" };

std::wstring_view VisualStudioGenerator::GetNamespace() const noexcept
{
    return Namespace;
}

std::wstring_view VisualStudioGenerator::GetDisplayName() const noexcept
{
    return RS_(L"VisualStudioGeneratorDisplayName");
}

std::wstring_view VisualStudioGenerator::GetIcon() const noexcept
{
    return IconHighRes;
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
