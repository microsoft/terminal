/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsDevCmdGenerator

Abstract:
- Dynamic profile generator for Visual Studio Developer Command Prompt

Author(s):
- Charles Willis - October 2020
- Heath Stewart - September 2021

--*/

#pragma once
#include "VisualStudioGenerator.h"
#include "VsSetupConfiguration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class VsDevCmdGenerator final : public VisualStudioGenerator::IVisualStudioProfileGenerator
    {
    public:
        void GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

    private:
        bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance&) const
        {
            // We only support version of VS from 15.0.
            // Per heaths: The [ISetupConfiguration] COM server only supports Visual Studio 15.0 and newer anyway.
            // Eliding the version range will improve the discovery performance by not having to parse or compare the versions.
            return true;
        }

        std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance) const
        {
            return L"VsDevCmd" + instance.GetInstanceId();
        }

        std::wstring GetProfileIconPath() const
        {
            return L"ms-appx:///ProfileIcons/{0caa0dad-35be-5f56-a8ff-afceeeaa6101}.png";
        }

        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const;
        std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const;
        std::wstring GetDevCmdScriptPath(const VsSetupConfiguration::VsSetupInstance& instance) const;
    };
};
