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

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class WslDistroGenerator final : public IDynamicProfileGenerator
    {
    public:
        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;
    };
};
