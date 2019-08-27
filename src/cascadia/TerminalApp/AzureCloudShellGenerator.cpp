/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
-

Abstract:
-
Author(s):
- Mike Griese - August 2019

--*/
#include "pch.h"

#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>

#include "AzureCloudShellGenerator.h"

#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include <io.h>
#include <fcntl.h>
#include "DefaultProfileUtils.h"

using namespace ::TerminalApp;

std::wstring_view AzureCloudShellGenerator::GetNamespace()
{
    return AzureGeneratorNamespace;
}

std::vector<TerminalApp::Profile> AzureCloudShellGenerator::GenerateProfiles()
{
    std::vector<TerminalApp::Profile> profiles{};

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
