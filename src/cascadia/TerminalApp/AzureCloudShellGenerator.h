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

static constexpr GUID AzureConnectionType = { 0xd9fcfdfa, 0xa479, 0x412c, { 0x83, 0xb7, 0xc5, 0x64, 0xe, 0x61, 0xcd, 0x62 } };

namespace TerminalApp
{
    class AzureCloudShellGenerator : public TerminalApp::IDynamicProfileGenerator
    {
    public:
        AzureCloudShellGenerator() = default;
        ~AzureCloudShellGenerator() = default;
        std::wstring_view GetNamespace() override;

        std::vector<TerminalApp::Profile> GenerateProfiles() override;
    };
};
