/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WslDistroGenerator

Abstract:
- This is the dynamic profile generator for WSL distros. Enumerates all the
  installed WSL distros to create profiles for them.

Author(s):
- Mike Griese - August 2019

--*/

#pragma once
#include "IDynamicProfileGenerator.h"

namespace TerminalApp
{
    class WslDistroGenerator : public TerminalApp::IDynamicProfileGenerator
    {
    public:
        WslDistroGenerator() = default;
        ~WslDistroGenerator() = default;
        std::wstring_view GetNamespace() override;
        std::vector<TerminalApp::Profile> GenerateProfiles() override;
    };
};
