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
#include "Profile.h"

namespace TerminalApp
{
    class IDynamicProfileGenerator;
};

class TerminalApp::IDynamicProfileGenerator
{
public:
    virtual ~IDynamicProfileGenerator() = 0;
    virtual std::wstring_view GetNamespace() = 0;
    virtual std::vector<TerminalApp::Profile> GenerateProfiles() = 0;
};
inline TerminalApp::IDynamicProfileGenerator::~IDynamicProfileGenerator() {}
