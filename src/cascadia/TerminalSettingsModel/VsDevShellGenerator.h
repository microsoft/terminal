/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsDevShellGenerator

Abstract:
- Dynamic profile generator for Visual Studio Developer PowerShell

Author(s):
- Charles Willis - October 2020

--*/

#pragma once
#include "BaseVisualStudioGenerator.h"

namespace Microsoft::Terminal::Settings::Model
{
    class VsDevShellGenerator : public BaseVisualStudioGenerator
    {
    public:
        VsDevShellGenerator() = default;
        ~VsDevShellGenerator() = default;

        std::wstring_view GetNamespace() override
        {
            return std::wstring_view{ L"Windows.Terminal.VisualStudio.Powershell" };
        }

        inline bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance instance) const override
        {
            return instance.VersionInRange(L"[16.2,)");
        }

        inline std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance instance) const override
        {
            return L"VsDevShell" + instance.GetInstanceId();
        }

        inline std::wstring GetProfileIconPath() const override
        {
            return L"ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png";
        }

        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance instance) const override;
        std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance instance) const override;
    };
};
