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

namespace Microsoft::Terminal::Settings::Model
{
    class BaseVisualStudioGenerator : public IDynamicProfileGenerator
    {
    public:
        // Inherited via IDynamicProfileGenerator
        std::wstring_view GetNamespace() override = 0;
        std::vector<winrt::Microsoft::Terminal::Settings::Model::Profile> GenerateProfiles() override;

    protected:
        virtual bool IsInstanceValid(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileCommandLine(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance) const = 0;
        virtual std::wstring GetProfileIconPath() const = 0;

    private:
        winrt::Microsoft::Terminal::Settings::Model::Profile CreateProfile(const std::wstring_view instanceId);
    };
};
