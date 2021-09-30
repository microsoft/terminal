/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsDevShellGenerator

Abstract:
- Dynamic profile generator for Visual Studio Developer PowerShell

Author(s):
- Charles Willis - October 2020
- Heath Stewart - September 2021

--*/

#pragma once
#include "VisualStudioGenerator.h"
#include "VsSetupConfiguration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class VsDevShellGenerator final : public VisualStudioGenerator::IVisualStudioProfileGenerator
    {
    public:
        void GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

    private:
        bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance& instance) const
        {
            return instance.VersionInRange(L"[16.2,)");
        }

        std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance) const
        {
            return L"VsDevShell" + instance.GetInstanceId();
        }

        std::wstring GetProfileIconPath() const
        {
            return L"ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png";
        }

        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const;
        std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const;
        std::wstring GetDevShellModulePath(const VsSetupConfiguration::VsSetupInstance& instance) const;
    };
};
