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

namespace Microsoft::Terminal::Settings::Model
{
    class WslDistroGenerator : public Microsoft::Terminal::Settings::Model::IDynamicProfileGenerator
    {
    public:
        WslDistroGenerator() = default;
        ~WslDistroGenerator() = default;
        std::wstring_view GetNamespace() override;
        std::vector<winrt::Microsoft::Terminal::Settings::Model::Profile> GenerateProfiles() override;
    };
};
