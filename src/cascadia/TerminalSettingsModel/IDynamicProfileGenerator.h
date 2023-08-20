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

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class IDynamicProfileGenerator
    {
    public:
        virtual ~IDynamicProfileGenerator() = default;
        virtual std::wstring_view GetNamespace() const noexcept = 0;
        virtual void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const = 0;
    };
};
