// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "AzureCloudShellGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"

#include "../../inc/DefaultSettings.h"
#include "DynamicProfileUtils.h"

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr winrt::guid AzureConnectionType = { 0xd9fcfdfa, 0xa479, 0x412c, { 0x83, 0xb7, 0xc5, 0x64, 0xe, 0x61, 0xcd, 0x62 } };

std::wstring_view AzureCloudShellGenerator::GetNamespace() const noexcept
{
    return AzureGeneratorNamespace;
}

// Method Description:
// - Checks if the Azure Cloud shell is available on this platform, and if it
//   is, creates a profile to be able to launch it.
// Arguments:
// - <none>
// Return Value:
// - a vector with the Azure Cloud Shell connection profile, if available.
void AzureCloudShellGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    auto azureCloudShellProfile{ CreateDynamicProfile(L"Azure Cloud Shell") };
    azureCloudShellProfile->StartingDirectory(winrt::hstring{ DEFAULT_STARTING_DIRECTORY });
    azureCloudShellProfile->DefaultAppearance().DarkColorSchemeName(L"Vintage");
    azureCloudShellProfile->DefaultAppearance().LightColorSchemeName(L"Vintage");
    azureCloudShellProfile->ConnectionType(AzureConnectionType);
    profiles.emplace_back(std::move(azureCloudShellProfile));
}
