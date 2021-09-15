/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- BaseVisualStudioGenerator

Abstract:
- Base generator for Visual Studio Developer shell profiles

Author(s):
- Charles Willis - October 2020

--*/

#pragma once

#include "IDynamicProfileGenerator.h"
#include "VsSetupConfiguration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class BaseVisualStudioGenerator : public IDynamicProfileGenerator
    {
    public:
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

    protected:
        virtual bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileIconPath() const = 0;
    };
};
