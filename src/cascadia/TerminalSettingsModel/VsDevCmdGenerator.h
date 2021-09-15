/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsDevCmdGenerator

Abstract:
- Dynamic profile generator for Visual Studio Developer Command Prompt

Author(s):
- Charles Willis - October 2020

--*/

#pragma once
#include "BaseVisualStudioGenerator.h"

namespace Microsoft::Terminal::Settings::Model
{
    class VsDevCmdGenerator : public BaseVisualStudioGenerator
    {
    public:
        VsDevCmdGenerator() = default;
        ~VsDevCmdGenerator() = default;

        std::wstring_view GetNamespace() override
        {
            return std::wstring_view{ L"Windows.Terminal.VisualStudio.CommandPrompt" };
        }

        inline bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance instance) const override
        {
            // We only support version of VS from 15.0.
            // Per heaths: The [ISetupConfiguration] COM server only supports Visual Studio 15.0 and newer anyway.
            // Eliding the version range will improve the discovery performance by not having to parse or compare the versions.
            return true;
        }

        inline std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance instance) const override
        {
            return L"VsDevCmd" + instance.GetInstanceId();
        }

        inline std::wstring GetProfileIconPath() const override
        {
            return L"ms-appx:///ProfileIcons/{0caa0dad-35be-5f56-a8ff-afceeeaa6101}.png";
        }

        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance instance) const override;
        std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance instance) const override;
    };
};
