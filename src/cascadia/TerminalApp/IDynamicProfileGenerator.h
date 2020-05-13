/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IDynamicProfileGenerator

Abstract:
- The DynamicProfileGenerator interface. A dynamic profile generator is a object
  that can synthesize a list of profiles based on some arbitrary, typically
  external criteria. Profiles from dynamic sources are only available in the
  user's profiles if the generator actually ran and created the profile.
- Each DPG must have a unique namespace to associate with itself. If the
  namespace is not unique, the generator risks affecting profiles from
  conflicting generators.

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
