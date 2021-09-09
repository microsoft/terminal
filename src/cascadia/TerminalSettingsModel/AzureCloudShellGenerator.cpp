// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "AzureCloudShellGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"

#include "../../inc/DefaultSettings.h"
#include "DefaultProfileUtils.h"

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

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
    if (AzureConnection::IsAzureConnectionAvailable())
    {
        auto azureCloudShellProfile{ CreateDefaultProfile(AzureGeneratorNamespace, L"Azure Cloud Shell") };
        azureCloudShellProfile->Commandline(L"Azure");
        azureCloudShellProfile->StartingDirectory(winrt::hstring{ DEFAULT_STARTING_DIRECTORY });
        azureCloudShellProfile->DefaultAppearance().ColorSchemeName(L"Vintage");
        azureCloudShellProfile->ConnectionType(AzureConnection::ConnectionType());
        profiles.emplace_back(std::move(azureCloudShellProfile));
    }
}
