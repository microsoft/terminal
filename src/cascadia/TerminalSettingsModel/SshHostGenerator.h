/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SshHostGenerator

Abstract:
- This is the dynamic profile generator for SSH connections. Enumerates all the
  SSH hosts to create profiles for them.

Author(s):
- Jon Thysell - September 2022

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class SshHostGenerator final : public IDynamicProfileGenerator
    {
    public:
        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;
    };
};
