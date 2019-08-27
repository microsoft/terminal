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

#pragma once
#include "IDynamicProfileGenerator.h"

namespace TerminalApp
{
    class WslDistroGenerator;
};

class TerminalApp::WslDistroGenerator : public TerminalApp::IDynamicProfileGenerator
{
public:
    WslDistroGenerator() = default;
    ~WslDistroGenerator() = default;
    std::wstring_view GetNamespace() override;
    std::vector<TerminalApp::Profile> GenerateProfiles() override;

    static constexpr std::wstring_view WslGeneratorNamespace{ L"Windows.Terminal.Wsl" };
};
