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

namespace Microsoft::Terminal::Settings::Model
{
    class AzureCloudShellGenerator : public Microsoft::Terminal::Settings::Model::IDynamicProfileGenerator
    {
    public:
        AzureCloudShellGenerator() = default;
        ~AzureCloudShellGenerator() = default;
        std::wstring_view GetNamespace() override;

        std::vector<winrt::Microsoft::Terminal::Settings::Model::Profile> GenerateProfiles() override;
    };
};
