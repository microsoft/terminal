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

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class PowershellCoreProfileGenerator final : public IDynamicProfileGenerator
    {
    public:
        static const std::wstring_view GetPreferredPowershellProfileName();

        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;
    };
};
