/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsDevCmdGenerator

Abstract:
- Dyanmic profile generator for Visual Studio Developer Command Prompt

Author(s):
- Charles Willis - October 2020

--*/

#pragma once
#include "BaseVisualStudioGenerator.h"

namespace TerminalApp
{
    class VsDevCmdGenerator :
        public TerminalApp::BaseVisualStudioGenerator
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
            return instance.VersionInRange(L"[15.0.0.0,)");
        }

        inline std::wstring GetProfileIconPath() const override
        {
            return L"ms-appx:///ProfileIcons/{0caa0dad-35be-5f56-a8ff-afceeeaa6101}.png";
        }

        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance instance) const override;
        std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance instance) const override;
    };
};
