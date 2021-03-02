/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PowershellCoreProfileGenerator

Abstract:
- This is the dynamic profile generator for PowerShell Core. Checks if pwsh is
  installed, and if it is, creates a profile to launch it.

Author(s):
- Mike Griese - August 2019

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace Microsoft::Terminal::Settings::Model
{
    class PowershellCoreProfileGenerator : public Microsoft::Terminal::Settings::Model::IDynamicProfileGenerator
    {
    public:
        static const std::wstring_view GetPreferredPowershellProfileName();

        PowershellCoreProfileGenerator() = default;
        ~PowershellCoreProfileGenerator() = default;
        std::wstring_view GetNamespace() override;

        std::vector<winrt::Microsoft::Terminal::Settings::Model::Profile> GenerateProfiles() override;
    };
};
