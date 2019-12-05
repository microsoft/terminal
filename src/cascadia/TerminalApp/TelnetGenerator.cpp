// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "TelnetGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"

#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include "DefaultProfileUtils.h"

using namespace ::TerminalApp;

std::wstring_view TelnetGenerator::GetNamespace()
{
    return TelnetGeneratorNamespace;
}

// Method Description:
// - Checks if the Telnet connector is available on this platform, and if it
//   is, creates a profile to be able to launch it.
// Arguments:
// - <none>
// Return Value:
// - a vector with the Telnet connection profile, if available.
std::vector<TerminalApp::Profile> TelnetGenerator::GenerateProfiles()
{
    std::vector<TerminalApp::Profile> profiles;

    auto telnetProfile{ CreateDefaultProfile(L"Telnet Loopback") };
    telnetProfile.SetCommandline(L"127.0.0.1");
    telnetProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    telnetProfile.SetColorScheme({ L"Vintage" });
    telnetProfile.SetAcrylicOpacity(1.0);
    telnetProfile.SetUseAcrylic(false);
    telnetProfile.SetConnectionType(TelnetConnectionType);
    profiles.emplace_back(telnetProfile);

    return profiles;
}
