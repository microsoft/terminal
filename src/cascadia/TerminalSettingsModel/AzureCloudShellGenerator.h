/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AzureCloudShellGenerator

Abstract:
- This is the dynamic profile generator for the azure cloud shell connector.
  Checks if the Azure Cloud shell is available on this platform, and if it is,
  creates a profile to be able to launch it.

Author(s):
- Mike Griese - August 2019

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class AzureCloudShellGenerator final : public IDynamicProfileGenerator
    {
    public:
        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;
    };
};
