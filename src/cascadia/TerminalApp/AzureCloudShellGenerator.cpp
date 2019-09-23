// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include <winrt/Microsoft.Terminal.TerminalConnection.h>

#include "AzureCloudShellGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"

#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

using namespace ::TerminalApp;

std::wstring_view AzureCloudShellGenerator::GetNamespace()
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
std::vector<TerminalApp::Profile> AzureCloudShellGenerator::GenerateProfiles()
{
    std::vector<TerminalApp::Profile> profiles;

    if (winrt::Microsoft::Terminal::TerminalConnection::AzureConnection::IsAzureConnectionAvailable())
    {
        auto azureCloudShellProfile{ CreateDefaultProfile(L"Azure Cloud Shell") };
        azureCloudShellProfile.SetCommandline(L"Azure");
        azureCloudShellProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        azureCloudShellProfile.SetColorScheme({ L"Vintage" });
        azureCloudShellProfile.SetAcrylicOpacity(0.6);
        azureCloudShellProfile.SetUseAcrylic(true);
        azureCloudShellProfile.SetCloseOnExit(false);
        azureCloudShellProfile.SetConnectionType(AzureConnectionType);
        profiles.emplace_back(azureCloudShellProfile);
    }

    return profiles;
}
