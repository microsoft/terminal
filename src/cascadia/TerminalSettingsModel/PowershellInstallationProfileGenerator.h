/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PowershellInstallationProfileGenerator

Abstract:
- This is the dynamic profile generator for a PowerShell stub. Checks if pwsh is
  installed, and if it is NOT installed, creates a profile that installs the
  latest PowerShell.

Author(s):
- Carlos Zamora - March 2025

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class PowershellInstallationProfileGenerator final : public IDynamicProfileGenerator
    {
    public:
        static std::wstring_view Namespace;
        std::wstring_view GetNamespace() const noexcept override;
        std::wstring_view GetDisplayName() const noexcept override;
        std::wstring_view GetIcon() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) override;
    };
};
