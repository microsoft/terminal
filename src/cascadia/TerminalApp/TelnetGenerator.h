/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TelnetGenerator

Abstract:
- This is the dynamic profile generator for the telnet connector.
  Checks if the Telnet connector is available on this platform, and if it is,
  creates a profile to be able to launch it.

Author(s):
- Michael Niksa - 2019-12-05
- Adapted from prior work by Mike Griese

--*/

#pragma once
#include "IDynamicProfileGenerator.h"

// {311153fb-d3f0-4ac6-b920-038de7cf5289}
static constexpr GUID TelnetConnectionType = { 0x311153fb, 0xd3f0, 0x4ac6, { 0xb9, 0x20, 0x03, 0x8d, 0xe7, 0xcf, 0x52, 0x89 } };

namespace TerminalApp
{
    class TelnetGenerator : public TerminalApp::IDynamicProfileGenerator
    {
    public:
        TelnetGenerator() = default;
        std::wstring_view GetNamespace() override;

        std::vector<TerminalApp::Profile> GenerateProfiles() override;
    };
};
